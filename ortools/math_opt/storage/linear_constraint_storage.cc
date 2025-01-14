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

#include "ortools/math_opt/storage/linear_constraint_storage.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

LinearConstraintId LinearConstraintStorage::Add(const double lower_bound,
                                                const double upper_bound,
                                                const absl::string_view name) {
  const LinearConstraintId id = next_id_++;
  Data& lin_con_data = linear_constraints_[id];
  lin_con_data.lower_bound = lower_bound;
  lin_con_data.upper_bound = upper_bound;
  lin_con_data.name = std::string(name);
  return id;
}

std::vector<LinearConstraintId> LinearConstraintStorage::LinearConstraints()
    const {
  std::vector<LinearConstraintId> result;
  for (const auto& [lin_con, _] : linear_constraints_) {
    result.push_back(lin_con);
  }
  return result;
}

std::vector<LinearConstraintId>
LinearConstraintStorage::SortedLinearConstraints() const {
  std::vector<LinearConstraintId> result = LinearConstraints();
  absl::c_sort(result);
  return result;
}

std::vector<LinearConstraintId> LinearConstraintStorage::ConstraintsFrom(
    const LinearConstraintId start) const {
  std::vector<LinearConstraintId> result;
  for (const LinearConstraintId c :
       util_intops::MakeStrongIntRange(start, next_id_)) {
    if (linear_constraints_.contains(c)) {
      result.push_back(c);
    }
  }
  return result;
}

std::pair<LinearConstraintsProto, SparseDoubleMatrixProto>
LinearConstraintStorage::Proto() const {
  LinearConstraintsProto constraints;
  const std::vector<LinearConstraintId> sorted_constraints =
      SortedLinearConstraints();
  for (const LinearConstraintId id : sorted_constraints) {
    AppendConstraint(id, &constraints);
  }
  return {constraints, matrix_.Proto()};
}

void LinearConstraintStorage::AppendConstraint(
    const LinearConstraintId constraint,
    LinearConstraintsProto* const proto) const {
  proto->add_ids(constraint.value());
  const Data& data = linear_constraints_.at(constraint);
  proto->add_lower_bounds(data.lower_bound);
  proto->add_upper_bounds(data.upper_bound);
  // TODO(b/238115672): we should potentially not fill this in on empty names.
  proto->add_names(data.name);
}

LinearConstraintsProto LinearConstraintStorage::Proto(
    const LinearConstraintId start, const LinearConstraintId end) const {
  LinearConstraintsProto result;
  for (const LinearConstraintId id :
       util_intops::MakeStrongIntRange(start, end)) {
    if (linear_constraints_.contains(id)) {
      AppendConstraint(id, &result);
    }
  }
  return result;
}

LinearConstraintStorage::Diff::Diff(const LinearConstraintStorage& storage,
                                    const VariableId variable_checkpoint)
    : checkpoint(storage.next_id()), variable_checkpoint(variable_checkpoint) {}

void LinearConstraintStorage::AdvanceCheckpointInDiff(
    const VariableId variable_checkpoint, Diff& diff) const {
  diff.checkpoint = std::max(diff.checkpoint, next_id_);
  diff.variable_checkpoint =
      std::max(diff.variable_checkpoint, variable_checkpoint);
  diff.deleted.clear();
  diff.lower_bounds.clear();
  diff.upper_bounds.clear();
  diff.matrix_keys.clear();
}

LinearConstraintStorage::UpdateResult LinearConstraintStorage::Update(
    const Diff& diff, const absl::flat_hash_set<VariableId>& deleted_variables,
    absl::Span<const VariableId> new_variables) const {
  UpdateResult result;
  for (const LinearConstraintId c : diff.deleted) {
    result.deleted.Add(c.value());
  }
  absl::c_sort(result.deleted);

  for (const LinearConstraintId c : SortedSetElements(diff.lower_bounds)) {
    result.updates.mutable_lower_bounds()->add_ids(c.value());
    result.updates.mutable_lower_bounds()->add_values(lower_bound(c));
  }

  for (const LinearConstraintId c : SortedSetElements(diff.upper_bounds)) {
    result.updates.mutable_upper_bounds()->add_ids(c.value());
    result.updates.mutable_upper_bounds()->add_values(upper_bound(c));
  }

  result.creates = Proto(diff.checkpoint, next_id_);

  result.matrix_updates =
      matrix_.Update(diff.deleted, ConstraintsFrom(diff.checkpoint),
                     deleted_variables, new_variables, diff.matrix_keys);
  return result;
}

}  // namespace operations_research::math_opt
