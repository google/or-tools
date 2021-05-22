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

#include "ortools/math_opt/core/indexed_model.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/int_type.h"
#include "ortools/math_opt/core/model_update_merge.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

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
absl::flat_hash_map<T, BasisStatus> SparseBasisVectorToMap(
    const SparseBasisStatusVector& sparse_vector) {
  absl::flat_hash_map<T, BasisStatus> result;
  CHECK_EQ(sparse_vector.ids_size(), sparse_vector.values_size());
  result.reserve(sparse_vector.ids_size());
  for (const auto [id, value] : MakeView(sparse_vector)) {
    gtl::InsertOrDie(&result, T(id), static_cast<BasisStatus>(value));
  }
  return result;
}

}  // namespace

VariableId IndexedModel::AddVariable(const double lower_bound,
                                     const double upper_bound,
                                     const bool is_integer,
                                     const absl::string_view name) {
  const VariableId result = next_variable_id_++;
  VariableData& var_data = variables_[result];
  var_data.lower_bound = lower_bound;
  var_data.upper_bound = upper_bound;
  var_data.is_integer = is_integer;
  var_data.name = name;
  if (!lazy_matrix_columns_.empty()) {
    gtl::InsertOrDie(&lazy_matrix_columns_, result, {});
  }
  return result;
}

void IndexedModel::DeleteVariable(const VariableId id) {
  CHECK(variables_.contains(id));
  EnsureLazyMatrixColumns();
  EnsureLazyMatrixRows();
  linear_objective_.erase(id);
  variables_.erase(id);
  if (id < variables_checkpoint_) {
    dirty_variable_deletes_.insert(id);
    dirty_variable_lower_bounds_.erase(id);
    dirty_variable_upper_bounds_.erase(id);
    dirty_variable_is_integer_.erase(id);
    dirty_linear_objective_coefficients_.erase(id);
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
}

std::vector<VariableId> IndexedModel::variables() const {
  return MapKeys(variables_);
}

std::vector<VariableId> IndexedModel::SortedVariables() const {
  return SortedMapKeys(variables_);
}

LinearConstraintId IndexedModel::AddLinearConstraint(
    const double lower_bound, const double upper_bound,
    const absl::string_view name) {
  const LinearConstraintId result = next_linear_constraint_id_++;
  LinearConstraintData& lin_con_data = linear_constraints_[result];
  lin_con_data.lower_bound = lower_bound;
  lin_con_data.upper_bound = upper_bound;
  lin_con_data.name = name;
  if (!lazy_matrix_rows_.empty()) {
    gtl::InsertOrDie(&lazy_matrix_rows_, result, {});
  }
  return result;
}

void IndexedModel::DeleteLinearConstraint(const LinearConstraintId id) {
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

std::vector<LinearConstraintId> IndexedModel::linear_constraints() const {
  return MapKeys(linear_constraints_);
}

std::vector<LinearConstraintId> IndexedModel::SortedLinearConstraints() const {
  return SortedMapKeys(linear_constraints_);
}

std::vector<VariableId> IndexedModel::SortedLinearObjectiveNonzeroVariables()
    const {
  return SortedMapKeys(linear_objective_);
}

void IndexedModel::AppendVariable(const VariableId id,
                                  VariablesProto& variables_proto) const {
  const VariableData& var_data = variables_.at(id);
  variables_proto.add_ids(id.value());
  variables_proto.add_lower_bounds(var_data.lower_bound);
  variables_proto.add_upper_bounds(var_data.upper_bound);
  variables_proto.add_integers(var_data.is_integer);
  variables_proto.add_names(var_data.name);
}

void IndexedModel::AppendLinearConstraint(
    const LinearConstraintId id,
    LinearConstraintsProto& linear_constraints_proto) const {
  const LinearConstraintData& con_impl = linear_constraints_.at(id);
  linear_constraints_proto.add_ids(id.value());
  linear_constraints_proto.add_lower_bounds(con_impl.lower_bound);
  linear_constraints_proto.add_upper_bounds(con_impl.upper_bound);
  linear_constraints_proto.add_names(con_impl.name);
}

void IndexedModel::ExportLinearConstraintMatrix(
    const absl::Span<const std::pair<LinearConstraintId, VariableId>> entries,
    SparseDoubleMatrixProto& matrix) const {
  matrix.mutable_row_ids()->Reserve(entries.size());
  matrix.mutable_column_ids()->Reserve(entries.size());
  matrix.mutable_coefficients()->Reserve(entries.size());
  for (const auto [constraint_id, variable_id] : entries) {
    matrix.add_row_ids(constraint_id.value());
    matrix.add_column_ids(variable_id.value());
    matrix.add_coefficients(gtl::FindWithDefault(linear_constraint_matrix_,
                                                 {constraint_id, variable_id}));
  }
}

ModelProto IndexedModel::ExportModel() const {
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

  // Pull out the linear constraints.
  for (const LinearConstraintId con : SortedMapKeys(linear_constraints_)) {
    AppendLinearConstraint(con, *result.mutable_linear_constraints());
  }

  // Pull out the constraint matrix.
  ExportLinearConstraintMatrix(SortedMapKeys(linear_constraint_matrix_),
                               *result.mutable_linear_constraint_matrix());
  return result;
}

absl::optional<ModelUpdateProto> IndexedModel::ExportSharedModelUpdate() {
  // We must detect the empty case to prevent unneeded copies and merging in
  // UpdateTracker::ExportModelUpdate().
  if (variables_checkpoint_ == next_variable_id_ &&
      linear_constraints_checkpoint_ == next_linear_constraint_id_ &&
      !dirty_objective_direction_ && !dirty_objective_offset_ &&
      dirty_variable_deletes_.empty() && dirty_variable_lower_bounds_.empty() &&
      dirty_variable_upper_bounds_.empty() &&
      dirty_variable_is_integer_.empty() &&
      dirty_linear_objective_coefficients_.empty() &&
      dirty_linear_constraint_deletes_.empty() &&
      dirty_linear_constraint_lower_bounds_.empty() &&
      dirty_linear_constraint_upper_bounds_.empty() &&
      dirty_linear_constraint_matrix_keys_.empty()) {
    return absl::nullopt;
  }

  // TODO(user): these are used to efficiently extract the constraint matrix
  // update, but it would be good to avoid calling these because they result in
  // a large allocation.
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
  ExportLinearConstraintMatrix(
      constraint_matrix_updates,
      *result.mutable_linear_constraint_matrix_updates());

  // Named returned value optimization (NRVO) does not apply here since the
  // return type if not the same type as `result`. To make things clear, we
  // explicitly call the constructor here.
  return {std::move(result)};
}

void IndexedModel::EnsureLazyMatrixColumns() {
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

void IndexedModel::EnsureLazyMatrixRows() {
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

void IndexedModel::SharedCheckpoint() {
  variables_checkpoint_ = next_variable_id_;
  linear_constraints_checkpoint_ = next_linear_constraint_id_;
  dirty_objective_direction_ = false;
  dirty_objective_offset_ = false;

  dirty_variable_deletes_.clear();
  dirty_variable_lower_bounds_.clear();
  dirty_variable_upper_bounds_.clear();
  dirty_variable_is_integer_.clear();

  dirty_linear_objective_coefficients_.clear();

  dirty_linear_constraint_deletes_.clear();
  dirty_linear_constraint_lower_bounds_.clear();
  dirty_linear_constraint_upper_bounds_.clear();
  dirty_linear_constraint_matrix_keys_.clear();
}

IndexedSolutions IndexedSolutionsFromProto(
    const SolveResultProto& solve_result) {
  IndexedSolutions solutions;
  for (const PrimalSolutionProto& primal_solution :
       solve_result.primal_solutions()) {
    IndexedPrimalSolution p;
    p.variable_values =
        MakeView(primal_solution.variable_values()).as_map<VariableId>();
    p.objective_value = primal_solution.objective_value();
    solutions.primal_solutions.push_back(std::move(p));
  }
  for (const PrimalRayProto& primal_ray : solve_result.primal_rays()) {
    IndexedPrimalRay pr;
    pr.variable_values =
        MakeView(primal_ray.variable_values()).as_map<VariableId>();
    solutions.primal_rays.push_back(std::move(pr));
  }
  for (const DualSolutionProto& dual_solution : solve_result.dual_solutions()) {
    IndexedDualSolution d;
    d.reduced_costs =
        MakeView(dual_solution.reduced_costs()).as_map<VariableId>();
    d.dual_values =
        MakeView(dual_solution.dual_values()).as_map<LinearConstraintId>();
    d.objective_value = dual_solution.objective_value();
    solutions.dual_solutions.push_back(std::move(d));
  }
  for (const DualRayProto& dual_ray : solve_result.dual_rays()) {
    IndexedDualRay dr;
    dr.reduced_costs = MakeView(dual_ray.reduced_costs()).as_map<VariableId>();
    dr.dual_values =
        MakeView(dual_ray.dual_values()).as_map<LinearConstraintId>();
    solutions.dual_rays.push_back(std::move(dr));
  }
  for (const BasisProto& basis : solve_result.basis()) {
    IndexedBasis indexed_basis;
    indexed_basis.constraint_status =
        SparseBasisVectorToMap<LinearConstraintId>(basis.constraint_status());
    indexed_basis.variable_status =
        SparseBasisVectorToMap<VariableId>(basis.variable_status());
    solutions.basis.push_back(std::move(indexed_basis));
  }
  return solutions;
}

std::unique_ptr<IndexedModel::UpdateTracker> IndexedModel::NewUpdateTracker() {
  // UpdateTracker constructor will call UpdateTracker::Checkpoint() that
  // flushes the current update to all other trackers and updates the checkpoint
  // of this model to the current state of the model as returned by
  // ExportModel().
  return absl::WrapUnique(new UpdateTracker(*this));
}

IndexedModel::UpdateTracker::UpdateTracker(IndexedModel& indexed_model)
    : indexed_model_(indexed_model) {
  absl::MutexLock lock(&indexed_model_.update_trackers_lock_);
  CHECK(indexed_model_.update_trackers_.insert(this).second);
  CheckpointLocked();
}

IndexedModel::UpdateTracker::~UpdateTracker() {
  absl::MutexLock lock(&indexed_model_.update_trackers_lock_);
  CHECK(indexed_model_.update_trackers_.erase(this));
}

absl::optional<ModelUpdateProto>
IndexedModel::UpdateTracker::ExportModelUpdate() {
  absl::MutexLock lock(&indexed_model_.update_trackers_lock_);

  // No updates have been pushed, the checkpoint of this tracker is in sync with
  // the shared checkpoint of IndexedModel. We can return the IndexedModel
  // shared update without merging.
  if (updates_.empty()) {
    return indexed_model_.ExportSharedModelUpdate();
  }

  // Find all trackers with the same checkpoint. By construction, all trackers
  // that have the same first update also share all next updates.
  std::vector<UpdateTracker*> all_trackers_at_checkpoint;
  bool found_this = false;
  for (UpdateTracker* const tracker : indexed_model_.update_trackers_) {
    if (!tracker->updates_.empty() &&
        tracker->updates_.front() == updates_.front()) {
      // Note that we set `found_this` inside the if branch to make sure we also
      // detect a bug in this code that would not include `this` in the list of
      // trackers.
      if (tracker == this) {
        found_this = true;
      }
      all_trackers_at_checkpoint.push_back(tracker);

      // Validate that we have the same updates in debug mode only. In optimized
      // mode, only test the size of the updates_ vectors.
      CHECK_EQ(updates_.size(), tracker->updates_.size());
      if (DEBUG_MODE) {
        for (int i = 0; i < updates_.size(); ++i) {
          CHECK_EQ(updates_[i], tracker->updates_[i])
              << "Two trackers have the same checkpoint but different updates.";
        }
      }
    }
  }
  CHECK(found_this);

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
  for (const auto& update : updates_) {
    MergeIntoUpdate(/*from=*/*update, /*into=*/*merge);
  }

  // Push the merge to all trackers that have the same checkpoint (including
  // this tracker).
  for (UpdateTracker* const tracker : all_trackers_at_checkpoint) {
    tracker->updates_.clear();
    tracker->updates_.push_back(merge);
  }

  ModelUpdateProto update = *merge;
  const absl::optional<ModelUpdateProto> pending_update =
      indexed_model_.ExportSharedModelUpdate();
  if (pending_update) {
    MergeIntoUpdate(/*from=*/*pending_update, /*into=*/update);
  }

  // Named returned value optimization (NRVO) does not apply here since the
  // return type if not the same type as `result`. To make things clear, we
  // explicitly call the constructor here.
  return {std::move(update)};
}

void IndexedModel::UpdateTracker::Checkpoint() {
  absl::MutexLock lock(&indexed_model_.update_trackers_lock_);

  CheckpointLocked();
}

void IndexedModel::UpdateTracker::CheckpointLocked() {
  // Optimize the case where we have a single tracker and we don't want to
  // update it. In that case we don't need to update trackers since we would
  // only update this one and clear it immediately.
  if (indexed_model_.update_trackers_.size() == 1) {
    CHECK(*indexed_model_.update_trackers_.begin() == this);
  } else {
    absl::optional<ModelUpdateProto> update =
        indexed_model_.ExportSharedModelUpdate();
    if (update) {
      const auto shared_update =
          std::make_shared<ModelUpdateProto>(*std::move(update));

      bool found_this = false;
      for (UpdateTracker* const tracker : indexed_model_.update_trackers_) {
        if (tracker == this) {
          found_this = true;
        }
        tracker->updates_.push_back(shared_update);
      }
      CHECK(found_this);
    }
  }
  indexed_model_.SharedCheckpoint();
  updates_.clear();
}

}  // namespace math_opt
}  // namespace operations_research
