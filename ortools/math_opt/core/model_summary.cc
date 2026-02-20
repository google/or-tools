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

#include "ortools/math_opt/core/model_summary.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research {
namespace math_opt {

namespace internal {

absl::Status CheckIdsRangeAndStrictlyIncreasing2(
    absl::Span<const int64_t> ids) {
  int64_t previous{-1};
  for (int i = 0; i < ids.size(); previous = ids[i], ++i) {
    if (ids[i] < 0 || ids[i] == std::numeric_limits<int64_t>::max()) {
      return util::InvalidArgumentErrorBuilder()
             << "Expected ids to be nonnegative and not max(int64_t) but at "
                "index "
             << i << " found id: " << ids[i];
    }
    if (ids[i] <= previous) {
      return util::InvalidArgumentErrorBuilder()
             << "Expected ids to be strictly increasing, but at index " << i
             << " found id: " << ids[i] << " and at previous index " << i - 1
             << " found id: " << ids[i - 1];
    }
  }
  return absl::OkStatus();
}

}  // namespace internal

IdNameBiMap::IdNameBiMap(
    std::initializer_list<std::pair<int64_t, absl::string_view>> ids)
    : IdNameBiMap(/*check_names=*/true) {
  for (const auto& pair : ids) {
    CHECK_OK(Insert(pair.first, std::string(pair.second)));
  }
}

IdNameBiMap::IdNameBiMap(const IdNameBiMap& other) { *this = other; }

IdNameBiMap& IdNameBiMap::operator=(const IdNameBiMap& other) {
  if (&other == this) {
    return *this;
  }
  next_free_id_ = other.next_free_id_;
  id_to_name_ = other.id_to_name_;
  if (!other.nonempty_name_to_id_.has_value()) {
    nonempty_name_to_id_ = std::nullopt;
  } else {
    nonempty_name_to_id_.emplace();
    for (const auto& [id, name] : id_to_name_) {
      if (!name.empty()) {
        const auto [it, success] =
            nonempty_name_to_id_->insert({absl::string_view(name), id});
        CHECK(success);  // CHECK is OK, other cannot have duplicate names and
                         // non nullopt nonempty_name_to_id_.
      }
    }
  }

  return *this;
}

absl::Status IdNameBiMap::BulkUpdate(
    absl::Span<const int64_t> deleted_ids, absl::Span<const int64_t> new_ids,
    const absl::Span<const std::string* const> names) {
  RETURN_IF_ERROR(internal::CheckIdsRangeAndStrictlyIncreasing2(deleted_ids))
      << "invalid deleted ids";
  RETURN_IF_ERROR(internal::CheckIdsRangeAndStrictlyIncreasing2(new_ids))
      << "invalid new ids";
  if (!names.empty() && names.size() != new_ids.size()) {
    return util::InvalidArgumentErrorBuilder()
           << "names had size " << names.size()
           << " but should either be empty of have size matching new_ids which "
              "has size "
           << new_ids.size();
  }
  for (const int64_t id : deleted_ids) {
    RETURN_IF_ERROR(Erase(id));
  }
  for (int i = 0; i < new_ids.size(); ++i) {
    RETURN_IF_ERROR(
        Insert(new_ids[i], names.empty() ? std::string{} : *names[i]));
  }
  return absl::OkStatus();
}

ModelSummary::ModelSummary(const bool check_names)
    : variables(check_names),
      auxiliary_objectives(check_names),
      linear_constraints(check_names),
      quadratic_constraints(check_names),
      second_order_cone_constraints(check_names),
      sos1_constraints(check_names),
      sos2_constraints(check_names),
      indicator_constraints(check_names) {}

absl::StatusOr<ModelSummary> ModelSummary::Create(const ModelProto& model,
                                                  const bool check_names) {
  ModelSummary summary(check_names);
  summary.maximize = model.objective().maximize();
  RETURN_IF_ERROR(summary.variables.BulkUpdate({}, model.variables().ids(),
                                               model.variables().names()))
      << "ModelProto.variables are invalid";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      {}, model.auxiliary_objectives(), summary.auxiliary_objectives))
      << "ModelProto.auxiliary_objectives are invalid";
  {
    absl::string_view objective_name = model.objective().name();
    if (summary.auxiliary_objectives.HasName(objective_name)) {
      return util::InvalidArgumentErrorBuilder()
             << "duplicate objective name: " << objective_name;
    }
    summary.primary_objective_name = objective_name;
  }
  RETURN_IF_ERROR(summary.linear_constraints.BulkUpdate(
      {}, model.linear_constraints().ids(), model.linear_constraints().names()))
      << "ModelProto.linear_constraints are invalid";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      {}, model.quadratic_constraints(), summary.quadratic_constraints))
      << "ModelProto.quadratic_constraints are invalid";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      {}, model.second_order_cone_constraints(),
      summary.second_order_cone_constraints))
      << "ModelProto.second_order_cone_constraints are invalid";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      {}, model.sos1_constraints(), summary.sos1_constraints))
      << "ModelProto.sos1_constraints are invalid";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      {}, model.sos2_constraints(), summary.sos2_constraints))
      << "ModelProto.sos2_constraints are invalid";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      {}, model.indicator_constraints(), summary.indicator_constraints))
      << "ModelProto.indicator_constraints are invalid";
  return summary;
}

absl::Status ModelSummary::Update(const ModelUpdateProto& model_update) {
  if (model_update.objective_updates().has_direction_update()) {
    maximize = model_update.objective_updates().direction_update();
  }
  RETURN_IF_ERROR(variables.BulkUpdate(model_update.deleted_variable_ids(),
                                       model_update.new_variables().ids(),
                                       model_update.new_variables().names()))
      << "invalid variables";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      model_update.auxiliary_objectives_updates().deleted_objective_ids(),
      model_update.auxiliary_objectives_updates().new_objectives(),
      auxiliary_objectives))
      << "invalid auxiliary objectives";
  if (auxiliary_objectives.HasName(primary_objective_name)) {
    return util::InvalidArgumentErrorBuilder()
           << "duplicate objective name: " << primary_objective_name;
  }
  RETURN_IF_ERROR(linear_constraints.BulkUpdate(
      model_update.deleted_linear_constraint_ids(),
      model_update.new_linear_constraints().ids(),
      model_update.new_linear_constraints().names()))
      << "invalid linear constraints";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      model_update.quadratic_constraint_updates().deleted_constraint_ids(),
      model_update.quadratic_constraint_updates().new_constraints(),
      quadratic_constraints))
      << "invalid quadratic constraints";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      model_update.second_order_cone_constraint_updates()
          .deleted_constraint_ids(),
      model_update.second_order_cone_constraint_updates().new_constraints(),
      second_order_cone_constraints))
      << "invalid second-order cone constraints";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      model_update.sos1_constraint_updates().deleted_constraint_ids(),
      model_update.sos1_constraint_updates().new_constraints(),
      sos1_constraints))
      << "invalid sos1 constraints";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      model_update.sos2_constraint_updates().deleted_constraint_ids(),
      model_update.sos2_constraint_updates().new_constraints(),
      sos2_constraints))
      << "invalid sos2 constraints";
  RETURN_IF_ERROR(internal::UpdateBiMapFromMappedData(
      model_update.indicator_constraint_updates().deleted_constraint_ids(),
      model_update.indicator_constraint_updates().new_constraints(),
      indicator_constraints))
      << "invalid indicator constraints";
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
