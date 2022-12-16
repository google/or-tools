// Copyright 2010-2025 Google LLC
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

#include "ortools/math_opt/storage/objective_storage.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

void ObjectiveStorage::Diff::SingleObjective::DeleteVariable(
    const VariableId deleted_variable, const VariableId variable_checkpoint,
    const SparseSymmetricMatrix& quadratic_terms) {
  if (deleted_variable >= variable_checkpoint) {
    return;
  }
  linear_coefficients.erase(deleted_variable);
  for (const VariableId v2 :
       quadratic_terms.RelatedVariables(deleted_variable)) {
    if (v2 < variable_checkpoint) {
      quadratic_coefficients.erase(
          {std::min(deleted_variable, v2), std::max(deleted_variable, v2)});
    }
  }
}

ObjectiveProto ObjectiveStorage::ObjectiveData::Proto() const {
  ObjectiveProto proto;
  proto.set_maximize(maximize);
  proto.set_priority(priority);
  proto.set_offset(offset);
  *proto.mutable_linear_coefficients() = linear_terms.Proto();
  *proto.mutable_quadratic_coefficients() = quadratic_terms.Proto();
  proto.set_name(name);
  return proto;
}

AuxiliaryObjectiveId ObjectiveStorage::AddAuxiliaryObjective(
    const int64_t priority, const absl::string_view name) {
  const AuxiliaryObjectiveId id = next_id_;
  ++next_id_;
  CHECK(!auxiliary_objectives_.contains(id));
  {
    ObjectiveData& data = auxiliary_objectives_[id];
    data.priority = priority;
    data.name = std::string(name);
  }
  return id;
}

std::vector<AuxiliaryObjectiveId> ObjectiveStorage::AuxiliaryObjectives()
    const {
  std::vector<AuxiliaryObjectiveId> ids;
  ids.reserve(num_auxiliary_objectives());
  for (const auto& [id, unused] : auxiliary_objectives_) {
    ids.push_back(id);
  }
  return ids;
}

std::vector<AuxiliaryObjectiveId> ObjectiveStorage::SortedAuxiliaryObjectives()
    const {
  std::vector<AuxiliaryObjectiveId> ids = AuxiliaryObjectives();
  absl::c_sort(ids);
  return ids;
}

std::pair<ObjectiveProto, google::protobuf::Map<int64_t, ObjectiveProto>>
ObjectiveStorage::Proto() const {
  google::protobuf::Map<int64_t, ObjectiveProto> auxiliary_objectives;
  for (const auto& [id, objective] : auxiliary_objectives_) {
    auxiliary_objectives[id.value()] = objective.Proto();
  }
  return {primary_objective_.Proto(), std::move(auxiliary_objectives)};
}

namespace {

void EnsureHasValue(std::optional<ObjectiveUpdatesProto>& update) {
  if (!update.has_value()) {
    update.emplace();
  }
}

}  // namespace

std::optional<ObjectiveUpdatesProto> ObjectiveStorage::ObjectiveData::Update(
    const Diff::SingleObjective& diff_data,
    const absl::flat_hash_set<VariableId>& deleted_variables,
    absl::Span<const VariableId> new_variables) const {
  std::optional<ObjectiveUpdatesProto> update_proto;

  if (diff_data.direction) {
    EnsureHasValue(update_proto);
    update_proto->set_direction_update(maximize);
  }
  if (diff_data.priority) {
    EnsureHasValue(update_proto);
    update_proto->set_priority_update(priority);
  }
  if (diff_data.offset) {
    EnsureHasValue(update_proto);
    update_proto->set_offset_update(offset);
  }
  for (const VariableId v : SortedSetElements(diff_data.linear_coefficients)) {
    EnsureHasValue(update_proto);
    update_proto->mutable_linear_coefficients()->add_ids(v.value());
    update_proto->mutable_linear_coefficients()->add_values(
        linear_terms.get(v));
  }
  for (const VariableId v : new_variables) {
    const double val = linear_terms.get(v);
    if (val != 0.0) {
      EnsureHasValue(update_proto);
      update_proto->mutable_linear_coefficients()->add_ids(v.value());
      update_proto->mutable_linear_coefficients()->add_values(val);
    }
  }
  SparseDoubleMatrixProto quadratic_update = quadratic_terms.Update(
      deleted_variables, new_variables, diff_data.quadratic_coefficients);
  if (!quadratic_update.row_ids().empty()) {
    // Do not set the field if there is no quadratic term changes.
    EnsureHasValue(update_proto);
    *update_proto->mutable_quadratic_coefficients() =
        std::move(quadratic_update);
  }
  return update_proto;
}

std::pair<ObjectiveUpdatesProto, AuxiliaryObjectivesUpdatesProto>
ObjectiveStorage::Update(
    const Diff& diff, const absl::flat_hash_set<VariableId>& deleted_variables,
    absl::Span<const VariableId> new_variables) const {
  AuxiliaryObjectivesUpdatesProto auxiliary_result;

  for (const AuxiliaryObjectiveId id : diff.deleted) {
    auxiliary_result.add_deleted_objective_ids(id.value());
  }
  absl::c_sort(*auxiliary_result.mutable_deleted_objective_ids());

  for (const auto& [id, objective] : auxiliary_objectives_) {
    // Note that any `Delete()`d objective will not be in the `objectives_` map.
    // Hence, each entry is either new (if not extracted) or potentially an
    // update on an existing objective.
    if (!diff.objective_tracked(id)) {
      // An un-extracted objective goes in the `new_objectives` map. It is fresh
      // and so there is no need to update, so we continue.
      (*auxiliary_result.mutable_new_objectives())[id.value()] =
          auxiliary_objectives_.at(id).Proto();
      continue;
    }

    // `Diff` provides no guarantees on which objectives will have entries in
    // `objective_diffs`; a missing entry is equivalent to one with an empty
    // key.
    std::optional<ObjectiveUpdatesProto> update_proto =
        objective.Update(gtl::FindWithDefault(diff.objective_diffs, id),
                         deleted_variables, new_variables);
    if (update_proto.has_value()) {
      // If the update message is empty we do not export it. This is
      // particularly important for auxiliary objectives as we do not want to
      // add empty map entries.
      (*auxiliary_result.mutable_objective_updates())[id.value()] =
          *std::move(update_proto);
    }
  }

  return {primary_objective_
              .Update(gtl::FindWithDefault(diff.objective_diffs,
                                           kPrimaryObjectiveId),
                      deleted_variables, new_variables)
              .value_or(ObjectiveUpdatesProto{}),
          std::move(auxiliary_result)};
}

void ObjectiveStorage::AdvanceCheckpointInDiff(
    const VariableId variable_checkpoint, Diff& diff) const {
  diff.objective_checkpoint = std::max(diff.objective_checkpoint, next_id_);
  diff.variable_checkpoint =
      std::max(diff.variable_checkpoint, variable_checkpoint);
  diff.objective_diffs.clear();
  diff.deleted.clear();
}

}  // namespace operations_research::math_opt
