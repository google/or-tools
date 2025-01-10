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

#include "ortools/math_opt/io/lp/model_utils.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/io/lp/lp_model.h"

namespace operations_research::lp_format {

namespace {

absl::StatusOr<LpModel> ReorderVariables(
    const LpModel& model,
    const util_intops::StrongVector<VariableIndex, VariableIndex>& new_to_old,
    const bool allow_skip_old) {
  constexpr VariableIndex kBad(-1);
  util_intops::StrongVector<VariableIndex, VariableIndex> old_to_new(
      model.variables().end_index(), kBad);
  for (const VariableIndex v_new : new_to_old.index_range()) {
    const VariableIndex v_old = new_to_old[v_new];
    if (v_old < VariableIndex(0) || v_old >= model.variables().end_index()) {
      return util::InvalidArgumentErrorBuilder()
             << "values of new_to_old be in [0," << model.variables().size()
             << "), found: " << v_old;
    }
    if (old_to_new[v_old] != kBad) {
      return util::InvalidArgumentErrorBuilder()
             << "found value: " << v_old << " twice in new_to_old";
    }
    old_to_new[v_old] = v_new;
  }
  if (!allow_skip_old) {
    for (const VariableIndex v_old : old_to_new.index_range()) {
      if (old_to_new[v_old] == kBad) {
        return util::InvalidArgumentErrorBuilder()
               << "no new VariableIndex for old VariableIndex: " << v_old;
      }
    }
  }
  LpModel result;
  for (const VariableIndex new_var : new_to_old.index_range()) {
    RETURN_IF_ERROR(
        result.AddVariable(model.variables()[new_to_old[new_var]]).status())
        << "should be unreachable";
  }
  // We build the constraints in the new model by iterating over the constraints
  // in the old model, copying each constraint and then modifying it in place.
  for (Constraint c : model.constraints()) {
    for (auto& [unused, var] : c.terms) {
      if (old_to_new[var] == kBad) {
        return util::InvalidArgumentErrorBuilder()
               << "variable " << var
               << " appears in a constraint but is not new_to_old";
      }
      var = old_to_new[var];
    }
    RETURN_IF_ERROR(result.AddConstraint(c).status())
        << "should be unreachable";
  }
  return result;
}

}  // namespace

LpModel RemoveUnusedVariables(const LpModel& model) {
  util_intops::StrongVector<VariableIndex, bool> old_vars_used(
      model.variables().end_index());
  for (const Constraint& c : model.constraints()) {
    for (const auto& [unused, var] : c.terms) {
      old_vars_used[var] = true;
    }
  }
  util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old;
  for (VariableIndex v_old : old_vars_used.index_range()) {
    if (old_vars_used[v_old]) {
      new_to_old.push_back(v_old);
    }
  }
  auto result = ReorderVariables(model, new_to_old, /*allow_skip_old=*/true);
  CHECK_OK(result);
  return *std::move(result);
}

absl::StatusOr<LpModel> PermuteVariables(
    const LpModel& model,
    const util_intops::StrongVector<VariableIndex, VariableIndex>&
        new_index_to_old_index) {
  return ReorderVariables(model, new_index_to_old_index,
                          /*allow_skip_old=*/false);
}

absl::StatusOr<LpModel> PermuteVariables(
    const LpModel& model,
    const util_intops::StrongVector<VariableIndex, std::string>&
        order_by_name) {
  util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old;
  for (const std::string& name : order_by_name) {
    auto it = model.variable_names().find(name);
    if (it == model.variable_names().end()) {
      return util::InvalidArgumentErrorBuilder()
             << "no variable with name: " << name << " in model";
    }
    new_to_old.push_back(it->second);
  }
  return PermuteVariables(model, new_to_old);
}

}  // namespace operations_research::lp_format
