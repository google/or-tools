// Copyright 2010-2024 Google LLC
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
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/io/names_removal.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/iterators.h"
#include "ortools/math_opt/storage/linear_constraint_storage.h"
#include "ortools/math_opt/storage/sparse_matrix.h"
#include "ortools/math_opt/storage/update_trackers.h"
#include "ortools/math_opt/storage/variable_storage.h"
#include "ortools/math_opt/validators/model_validator.h"

namespace operations_research {
namespace math_opt {

absl::StatusOr<std::unique_ptr<ModelStorage>> ModelStorage::FromModelProto(
    const ModelProto& model_proto) {
  // We don't check names since ModelStorage does not do so before exporting
  // models. Thus a model built by ModelStorage can contain duplicated
  // names. And since we use FromModelProto() to implement Clone(), we must make
  // sure duplicated names don't fail.
  RETURN_IF_ERROR(ValidateModel(model_proto, /*check_names=*/false).status());

  auto storage = std::make_unique<ModelStorage>(model_proto.name(),
                                                model_proto.objective().name());

  // Add variables.
  storage->AddVariables(model_proto.variables());

  // Set the objective.
  storage->set_is_maximize(kPrimaryObjectiveId,
                           model_proto.objective().maximize());
  storage->set_objective_offset(kPrimaryObjectiveId,
                                model_proto.objective().offset());
  storage->UpdateLinearObjectiveCoefficients(
      kPrimaryObjectiveId, model_proto.objective().linear_coefficients());
  storage->UpdateQuadraticObjectiveCoefficients(
      kPrimaryObjectiveId, model_proto.objective().quadratic_coefficients());

  // Set the auxiliary objectives.
  storage->AddAuxiliaryObjectives(model_proto.auxiliary_objectives());

  // Add linear constraints.
  storage->AddLinearConstraints(model_proto.linear_constraints());

  // Set the linear constraints coefficients.
  storage->UpdateLinearConstraintCoefficients(
      model_proto.linear_constraint_matrix());

  // Add quadratic constraints.
  storage->quadratic_constraints_.AddConstraints(
      model_proto.quadratic_constraints());

  // Add SOC constraints.
  storage->soc_constraints_.AddConstraints(
      model_proto.second_order_cone_constraints());

  // Add SOS constraints.
  storage->sos1_constraints_.AddConstraints(model_proto.sos1_constraints());
  storage->sos2_constraints_.AddConstraints(model_proto.sos2_constraints());

  // Add indicator constraints.
  storage->indicator_constraints_.AddConstraints(
      model_proto.indicator_constraints());

  return storage;
}

void ModelStorage::UpdateObjective(const ObjectiveId id,
                                   const ObjectiveUpdatesProto& update) {
  if (update.has_direction_update()) {
    set_is_maximize(id, update.direction_update());
  }
  if (update.has_priority_update()) {
    set_objective_priority(id, update.priority_update());
  }
  if (update.has_offset_update()) {
    set_objective_offset(id, update.offset_update());
  }
  UpdateLinearObjectiveCoefficients(id, update.linear_coefficients());
  UpdateQuadraticObjectiveCoefficients(id, update.quadratic_coefficients());
}

void ModelStorage::UpdateLinearObjectiveCoefficients(
    const ObjectiveId id, const SparseDoubleVectorProto& coefficients) {
  for (const auto [var_id, value] : MakeView(coefficients)) {
    set_linear_objective_coefficient(id, VariableId(var_id), value);
  }
}

void ModelStorage::UpdateQuadraticObjectiveCoefficients(
    const ObjectiveId id, const SparseDoubleMatrixProto& coefficients) {
  for (int i = 0; i < coefficients.row_ids_size(); ++i) {
    // This call is valid since this is an upper triangular matrix; there is no
    // duplicated terms.
    set_quadratic_objective_coefficient(id, VariableId(coefficients.row_ids(i)),
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
  clone.value()->ensure_next_variable_id_at_least(next_variable_id());
  clone.value()->ensure_next_auxiliary_objective_id_at_least(
      next_auxiliary_objective_id());
  clone.value()->ensure_next_linear_constraint_id_at_least(
      next_linear_constraint_id());
  clone.value()->ensure_next_constraint_id_at_least(
      next_constraint_id<QuadraticConstraintId>());
  clone.value()->ensure_next_constraint_id_at_least(
      next_constraint_id<SecondOrderConeConstraintId>());
  clone.value()->ensure_next_constraint_id_at_least(
      next_constraint_id<Sos1ConstraintId>());
  clone.value()->ensure_next_constraint_id_at_least(
      next_constraint_id<Sos2ConstraintId>());
  clone.value()->ensure_next_constraint_id_at_least(
      next_constraint_id<IndicatorConstraintId>());

  return std::move(clone).value();
}

VariableId ModelStorage::AddVariable(const double lower_bound,
                                     const double upper_bound,
                                     const bool is_integer,
                                     const absl::string_view name) {
  return variables_.Add(lower_bound, upper_bound, is_integer, name);
}

void ModelStorage::AddVariables(const VariablesProto& variables) {
  const bool has_names = !variables.names().empty();
  for (int v = 0; v < variables.ids_size(); ++v) {
    // Make sure the ids of the new Variables in the model match the proto,
    // which are potentially non-consecutive (note that variables has been
    // validated).
    ensure_next_variable_id_at_least(VariableId(variables.ids(v)));
    AddVariable(variables.lower_bounds(v),
                /*upper_bound=*/variables.upper_bounds(v),
                /*is_integer=*/variables.integers(v),
                has_names ? variables.names(v) : absl::string_view());
  }
}

void ModelStorage::DeleteVariable(const VariableId id) {
  CHECK(variables_.contains(id));
  const auto& trackers = update_trackers_.GetUpdatedTrackers();
  // Reuse output of GetUpdatedTrackers() only once to ensure a consistent view,
  // do not call UpdateAndGetLinearConstraintDiffs() etc.
  objectives_.DeleteVariable(
      id,
      MakeUpdateDataFieldRange<&UpdateTrackerData::dirty_objective>(trackers));
  linear_constraints_.DeleteVariable(
      id,
      MakeUpdateDataFieldRange<&UpdateTrackerData::dirty_linear_constraints>(
          trackers));
  quadratic_constraints_.DeleteVariable(id);
  soc_constraints_.DeleteVariable(id);
  sos1_constraints_.DeleteVariable(id);
  sos2_constraints_.DeleteVariable(id);
  indicator_constraints_.DeleteVariable(id);
  variables_.Delete(
      id,
      MakeUpdateDataFieldRange<&UpdateTrackerData::dirty_variables>(trackers));
}

std::vector<VariableId> ModelStorage::variables() const {
  return variables_.Variables();
}

std::vector<VariableId> ModelStorage::SortedVariables() const {
  return variables_.SortedVariables();
}

LinearConstraintId ModelStorage::AddLinearConstraint(
    const double lower_bound, const double upper_bound,
    const absl::string_view name) {
  return linear_constraints_.Add(lower_bound, upper_bound, name);
}

void ModelStorage::AddLinearConstraints(
    const LinearConstraintsProto& linear_constraints) {
  const bool has_names = !linear_constraints.names().empty();
  for (int c = 0; c < linear_constraints.ids_size(); ++c) {
    // Make sure the ids of the new linear constraints in the model match the
    // proto, which are potentially non-consecutive (note that
    // linear_constraints has been validated).
    ensure_next_linear_constraint_id_at_least(
        LinearConstraintId(linear_constraints.ids(c)));
    // This call is valid since ids are unique and increasing.
    AddLinearConstraint(
        /*lower_bound=*/linear_constraints.lower_bounds(c),
        /*upper_bound=*/linear_constraints.upper_bounds(c),
        has_names ? linear_constraints.names(c) : absl::string_view());
  }
}

void ModelStorage::DeleteLinearConstraint(const LinearConstraintId id) {
  CHECK(linear_constraints_.contains(id));
  linear_constraints_.Delete(id, UpdateAndGetLinearConstraintDiffs());
}

std::vector<LinearConstraintId> ModelStorage::LinearConstraints() const {
  return linear_constraints_.LinearConstraints();
}

std::vector<LinearConstraintId> ModelStorage::SortedLinearConstraints() const {
  return linear_constraints_.SortedLinearConstraints();
}

void ModelStorage::AddAuxiliaryObjectives(
    const google::protobuf::Map<int64_t, ObjectiveProto>& objectives) {
  for (const int64_t raw_id : SortedMapKeys(objectives)) {
    const AuxiliaryObjectiveId id = AuxiliaryObjectiveId(raw_id);
    ensure_next_auxiliary_objective_id_at_least(id);
    const ObjectiveProto& proto = objectives.at(raw_id);
    AddAuxiliaryObjective(proto.priority(), proto.name());
    set_is_maximize(id, proto.maximize());
    set_objective_offset(id, proto.offset());
    for (const auto [raw_var_id, coeff] :
         MakeView(proto.linear_coefficients())) {
      set_linear_objective_coefficient(id, VariableId(raw_var_id), coeff);
    }
  }
}

// TODO: b/315974557 - Return an error if any of the Proto() methods called
// tries to create a very long RepeatedField.
ModelProto ModelStorage::ExportModel(const bool remove_names) const {
  ModelProto result;
  result.set_name(name_);
  *result.mutable_variables() = variables_.Proto();
  {
    auto [primary, auxiliary] = objectives_.Proto();
    *result.mutable_objective() = std::move(primary);
    *result.mutable_auxiliary_objectives() = std::move(auxiliary);
  }
  {
    auto [constraints, matrix] = linear_constraints_.Proto();
    *result.mutable_linear_constraints() = std::move(constraints);
    *result.mutable_linear_constraint_matrix() = std::move(matrix);
  }
  *result.mutable_quadratic_constraints() = quadratic_constraints_.Proto();
  *result.mutable_second_order_cone_constraints() = soc_constraints_.Proto();
  *result.mutable_sos1_constraints() = sos1_constraints_.Proto();
  *result.mutable_sos2_constraints() = sos2_constraints_.Proto();
  *result.mutable_indicator_constraints() = indicator_constraints_.Proto();
  // Performance can be improved when remove_names is true by just not
  // extracting the names above instead of clearing them below, but this will
  // be more code, see discussion on cl/549469633 and prototype in cl/549369764.
  if (remove_names) {
    RemoveNames(result);
  }
  return result;
}

std::optional<ModelUpdateProto>
ModelStorage::UpdateTrackerData::ExportModelUpdate(
    const ModelStorage& storage, const bool remove_names) const {
  // We must detect the empty case to prevent unneeded copies and merging in
  // ExportModelUpdate().

  if (storage.variables_.diff_is_empty(dirty_variables) &&
      storage.objectives_.diff_is_empty(dirty_objective) &&
      storage.linear_constraints_.diff_is_empty(dirty_linear_constraints) &&
      storage.quadratic_constraints_.diff_is_empty(
          dirty_quadratic_constraints) &&
      storage.soc_constraints_.diff_is_empty(dirty_soc_constraints) &&
      storage.sos1_constraints_.diff_is_empty(dirty_sos1_constraints) &&
      storage.sos2_constraints_.diff_is_empty(dirty_sos2_constraints) &&
      storage.indicator_constraints_.diff_is_empty(
          dirty_indicator_constraints)) {
    return std::nullopt;
  }

  ModelUpdateProto result;

  // Variable/constraint deletions.
  {
    VariableStorage::UpdateResult variable_update =
        storage.variables_.Update(dirty_variables);
    *result.mutable_deleted_variable_ids() = std::move(variable_update.deleted);
    *result.mutable_variable_updates() = std::move(variable_update.updates);
    *result.mutable_new_variables() = std::move(variable_update.creates);
  }
  const std::vector<VariableId> new_variables =
      storage.variables_.VariablesFrom(dirty_variables.checkpoint);

  // Linear constraint updates
  {
    LinearConstraintStorage::UpdateResult lin_con_update =
        storage.linear_constraints_.Update(
            dirty_linear_constraints, dirty_variables.deleted, new_variables);
    *result.mutable_deleted_linear_constraint_ids() =
        std::move(lin_con_update.deleted);
    *result.mutable_linear_constraint_updates() =
        std::move(lin_con_update.updates);
    *result.mutable_new_linear_constraints() =
        std::move(lin_con_update.creates);
    *result.mutable_linear_constraint_matrix_updates() =
        std::move(lin_con_update.matrix_updates);
  }

  // Quadratic constraint updates
  *result.mutable_quadratic_constraint_updates() =
      storage.quadratic_constraints_.Update(dirty_quadratic_constraints);

  // Second-order cone constraint updates
  *result.mutable_second_order_cone_constraint_updates() =
      storage.soc_constraints_.Update(dirty_soc_constraints);

  // SOS constraint updates
  *result.mutable_sos1_constraint_updates() =
      storage.sos1_constraints_.Update(dirty_sos1_constraints);
  *result.mutable_sos2_constraint_updates() =
      storage.sos2_constraints_.Update(dirty_sos2_constraints);

  // Indicator constraint updates
  *result.mutable_indicator_constraint_updates() =
      storage.indicator_constraints_.Update(dirty_indicator_constraints);

  // Update the objective
  {
    auto [primary, auxiliary] = storage.objectives_.Update(
        dirty_objective, dirty_variables.deleted, new_variables);
    *result.mutable_objective_updates() = std::move(primary);
    *result.mutable_auxiliary_objectives_updates() = std::move(auxiliary);
  }
  if (remove_names) {
    RemoveNames(result);
  }
  // Note: Named returned value optimization (NRVO) does not apply here.
  return {std::move(result)};
}

void ModelStorage::UpdateTrackerData::AdvanceCheckpoint(
    const ModelStorage& storage) {
  storage.variables_.AdvanceCheckpointInDiff(dirty_variables);
  storage.objectives_.AdvanceCheckpointInDiff(dirty_variables.checkpoint,
                                              dirty_objective);
  storage.linear_constraints_.AdvanceCheckpointInDiff(
      dirty_variables.checkpoint, dirty_linear_constraints);
  storage.quadratic_constraints_.AdvanceCheckpointInDiff(
      dirty_quadratic_constraints);
  storage.soc_constraints_.AdvanceCheckpointInDiff(dirty_soc_constraints);
  storage.sos1_constraints_.AdvanceCheckpointInDiff(dirty_sos1_constraints);
  storage.sos2_constraints_.AdvanceCheckpointInDiff(dirty_sos2_constraints);
  storage.indicator_constraints_.AdvanceCheckpointInDiff(
      dirty_indicator_constraints);
}

UpdateTrackerId ModelStorage::NewUpdateTracker() {
  return update_trackers_.NewUpdateTracker(
      variables_, objectives_, linear_constraints_, quadratic_constraints_,
      soc_constraints_, sos1_constraints_, sos2_constraints_,
      indicator_constraints_);
}

void ModelStorage::DeleteUpdateTracker(const UpdateTrackerId update_tracker) {
  update_trackers_.DeleteUpdateTracker(update_tracker);
}

std::optional<ModelUpdateProto> ModelStorage::ExportModelUpdate(
    const UpdateTrackerId update_tracker, const bool remove_names) const {
  return update_trackers_.GetData(update_tracker)
      .ExportModelUpdate(*this, remove_names);
}

void ModelStorage::AdvanceCheckpoint(UpdateTrackerId update_tracker) {
  update_trackers_.GetData(update_tracker).AdvanceCheckpoint(*this);
}

absl::Status ModelStorage::ApplyUpdateProto(
    const ModelUpdateProto& update_proto) {
  // Check the update first.
  {
    // Do not check for duplicate names, as with FromModelProto();
    ModelSummary summary(/*check_names=*/false);
    // IdNameBiMap requires Insert() calls to be in sorted id order.
    for (const VariableId id : SortedVariables()) {
      RETURN_IF_ERROR(summary.variables.Insert(id.value(), variable_name(id)))
          << "invalid variable id in model";
    }
    RETURN_IF_ERROR(
        summary.variables.SetNextFreeId(variables_.next_id().value()));
    for (const AuxiliaryObjectiveId id : SortedAuxiliaryObjectives()) {
      RETURN_IF_ERROR(
          summary.auxiliary_objectives.Insert(id.value(), objective_name(id)))
          << "invalid auxiliary objective id in model";
    }
    RETURN_IF_ERROR(summary.auxiliary_objectives.SetNextFreeId(
        objectives_.next_id().value()));
    for (const LinearConstraintId id : SortedLinearConstraints()) {
      RETURN_IF_ERROR(summary.linear_constraints.Insert(
          id.value(), linear_constraint_name(id)))
          << "invalid linear constraint id in model";
    }
    RETURN_IF_ERROR(summary.linear_constraints.SetNextFreeId(
        linear_constraints_.next_id().value()));
    for (const auto id : SortedConstraints<QuadraticConstraintId>()) {
      RETURN_IF_ERROR(summary.quadratic_constraints.Insert(
          id.value(), quadratic_constraints_.data(id).name))
          << "invalid quadratic constraint id in model";
    }
    RETURN_IF_ERROR(summary.quadratic_constraints.SetNextFreeId(
        quadratic_constraints_.next_id().value()));
    for (const auto id : SortedConstraints<SecondOrderConeConstraintId>()) {
      RETURN_IF_ERROR(summary.second_order_cone_constraints.Insert(
          id.value(), soc_constraints_.data(id).name))
          << "invalid second-order cone constraint id in model";
    }
    RETURN_IF_ERROR(summary.second_order_cone_constraints.SetNextFreeId(
        soc_constraints_.next_id().value()));
    for (const Sos1ConstraintId id : SortedConstraints<Sos1ConstraintId>()) {
      RETURN_IF_ERROR(summary.sos1_constraints.Insert(
          id.value(), constraint_data(id).name()))
          << "invalid SOS1 constraint id in model";
    }
    RETURN_IF_ERROR(summary.sos1_constraints.SetNextFreeId(
        sos1_constraints_.next_id().value()));
    for (const Sos2ConstraintId id : SortedConstraints<Sos2ConstraintId>()) {
      RETURN_IF_ERROR(summary.sos2_constraints.Insert(
          id.value(), constraint_data(id).name()))
          << "invalid SOS2 constraint id in model";
    }
    RETURN_IF_ERROR(summary.sos2_constraints.SetNextFreeId(
        sos2_constraints_.next_id().value()));

    for (const IndicatorConstraintId id :
         SortedConstraints<IndicatorConstraintId>()) {
      RETURN_IF_ERROR(summary.indicator_constraints.Insert(
          id.value(), constraint_data(id).name));
    }
    RETURN_IF_ERROR(summary.indicator_constraints.SetNextFreeId(
        indicator_constraints_.next_id().value()));

    RETURN_IF_ERROR(ValidateModelUpdate(update_proto, summary))
        << "update not valid";
  }

  // Remove deleted variables and constraints.
  for (const int64_t v_id : update_proto.deleted_variable_ids()) {
    DeleteVariable(VariableId(v_id));
  }
  for (const int64_t o_id :
       update_proto.auxiliary_objectives_updates().deleted_objective_ids()) {
    DeleteAuxiliaryObjective(AuxiliaryObjectiveId(o_id));
  }
  for (const int64_t c_id : update_proto.deleted_linear_constraint_ids()) {
    DeleteLinearConstraint(LinearConstraintId(c_id));
  }
  for (const int64_t c_id :
       update_proto.quadratic_constraint_updates().deleted_constraint_ids()) {
    DeleteAtomicConstraint(QuadraticConstraintId(c_id));
  }
  for (const int64_t c_id : update_proto.second_order_cone_constraint_updates()
                                .deleted_constraint_ids()) {
    DeleteAtomicConstraint(SecondOrderConeConstraintId(c_id));
  }
  for (const int64_t c_id :
       update_proto.sos1_constraint_updates().deleted_constraint_ids()) {
    DeleteAtomicConstraint(Sos1ConstraintId(c_id));
  }
  for (const int64_t c_id :
       update_proto.sos2_constraint_updates().deleted_constraint_ids()) {
    DeleteAtomicConstraint(Sos2ConstraintId(c_id));
  }
  for (const int64_t c_id :
       update_proto.indicator_constraint_updates().deleted_constraint_ids()) {
    DeleteAtomicConstraint(IndicatorConstraintId(c_id));
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
  AddAuxiliaryObjectives(
      update_proto.auxiliary_objectives_updates().new_objectives());
  AddLinearConstraints(update_proto.new_linear_constraints());
  quadratic_constraints_.AddConstraints(
      update_proto.quadratic_constraint_updates().new_constraints());
  soc_constraints_.AddConstraints(
      update_proto.second_order_cone_constraint_updates().new_constraints());
  sos1_constraints_.AddConstraints(
      update_proto.sos1_constraint_updates().new_constraints());
  sos2_constraints_.AddConstraints(
      update_proto.sos2_constraint_updates().new_constraints());
  indicator_constraints_.AddConstraints(
      update_proto.indicator_constraint_updates().new_constraints());

  // Update the primary objective.
  UpdateObjective(kPrimaryObjectiveId, update_proto.objective_updates());

  // Update the auxiliary objectives.
  for (const auto& [raw_id, objective_update] :
       update_proto.auxiliary_objectives_updates().objective_updates()) {
    UpdateObjective(AuxiliaryObjectiveId(raw_id), objective_update);
  }

  // Update the linear constraints' coefficients.
  UpdateLinearConstraintCoefficients(
      update_proto.linear_constraint_matrix_updates());

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
