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

#include "ortools/math_opt/storage/variable_storage.h"

#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

VariableId VariableStorage::Add(const double lower_bound,
                                const double upper_bound, const bool is_integer,
                                const absl::string_view name) {
  const VariableId id = next_variable_id_;
  VariableData& var_data = variables_[id];
  var_data.lower_bound = lower_bound;
  var_data.upper_bound = upper_bound;
  var_data.is_integer = is_integer;
  var_data.name = std::string(name);
  ++next_variable_id_;
  return id;
}

std::vector<VariableId> VariableStorage::Variables() const {
  std::vector<VariableId> result;
  result.reserve(variables_.size());
  for (const auto& [var, _] : variables_) {
    result.push_back(var);
  }
  return result;
}

std::vector<VariableId> VariableStorage::SortedVariables() const {
  std::vector<VariableId> result = Variables();
  absl::c_sort(result);
  return result;
}

std::vector<VariableId> VariableStorage::VariablesFrom(
    const VariableId start) const {
  std::vector<VariableId> result;
  for (const VariableId v :
       util_intops::MakeStrongIntRange(start, next_variable_id_)) {
    if (variables_.contains(v)) {
      result.push_back(v);
    }
  }
  return result;
}

void VariableStorage::AppendVariable(const VariableId variable,
                                     VariablesProto* proto) const {
  const VariableData& data = variables_.at(variable);
  proto->add_ids(variable.value());
  proto->add_lower_bounds(data.lower_bound);
  proto->add_upper_bounds(data.upper_bound);
  proto->add_integers(data.is_integer);
  proto->add_names(data.name);
}

VariablesProto VariableStorage::Proto() const {
  VariablesProto result;
  for (const VariableId v : SortedVariables()) {
    AppendVariable(v, &result);
  }
  return result;
}

void VariableStorage::AdvanceCheckpointInDiff(Diff& diff) const {
  diff.checkpoint = next_variable_id_;
  diff.deleted.clear();
  diff.lower_bounds.clear();
  diff.upper_bounds.clear();
  diff.integer.clear();
}

VariableStorage::UpdateResult VariableStorage::Update(const Diff& diff) const {
  UpdateResult result;
  for (const VariableId v : diff.deleted) {
    result.deleted.Add(v.value());
  }
  absl::c_sort(result.deleted);
  for (const VariableId v : SortedSetElements(diff.lower_bounds)) {
    result.updates.mutable_lower_bounds()->add_ids(v.value());
    result.updates.mutable_lower_bounds()->add_values(lower_bound(v));
  }
  for (const VariableId v : SortedSetElements(diff.upper_bounds)) {
    result.updates.mutable_upper_bounds()->add_ids(v.value());
    result.updates.mutable_upper_bounds()->add_values(upper_bound(v));
  }
  for (const VariableId v : SortedSetElements(diff.integer)) {
    result.updates.mutable_integers()->add_ids(v.value());
    result.updates.mutable_integers()->add_values(is_integer(v));
  }
  for (const VariableId v :
       util_intops::MakeStrongIntRange(diff.checkpoint, next_variable_id_)) {
    if (variables_.contains(v)) {
      AppendVariable(v, &result.creates);
    }
  }
  return result;
}

}  // namespace operations_research::math_opt
