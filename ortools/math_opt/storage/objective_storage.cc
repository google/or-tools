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

#include "ortools/math_opt/storage/objective_storage.h"

#include <algorithm>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/sorted.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

ObjectiveProto ObjectiveStorage::Proto() const {
  ObjectiveProto result;
  result.set_maximize(maximize_);
  result.set_offset(offset_);
  *result.mutable_linear_coefficients() = linear_terms_.Proto();
  *result.mutable_quadratic_coefficients() = quadratic_terms_.Proto();
  return result;
}

ObjectiveUpdatesProto ObjectiveStorage::Update(
    const Diff& diff, const absl::flat_hash_set<VariableId>& deleted_variables,
    const std::vector<VariableId>& new_variables) const {
  ObjectiveUpdatesProto result;
  if (diff.direction) {
    result.set_direction_update(maximize_);
  }
  if (diff.offset) {
    result.set_offset_update(offset_);
  }

  for (const VariableId v : SortedElements(diff.linear_coefficients)) {
    result.mutable_linear_coefficients()->add_ids(v.value());
    result.mutable_linear_coefficients()->add_values(linear_term(v));
  }
  for (const VariableId v : new_variables) {
    const double val = linear_term(v);
    if (val != 0.0) {
      result.mutable_linear_coefficients()->add_ids(v.value());
      result.mutable_linear_coefficients()->add_values(val);
    }
  }
  *result.mutable_quadratic_coefficients() = quadratic_terms_.Update(
      deleted_variables, new_variables, diff.quadratic_coefficients);
  return result;
}

void ObjectiveStorage::AdvanceCheckpointInDiff(
    const VariableId variable_checkpoint, Diff& diff) const {
  diff.variable_checkpoint =
      std::max(diff.variable_checkpoint, variable_checkpoint);
  diff.offset = false;
  diff.direction = false;
  diff.linear_coefficients.clear();
  diff.quadratic_coefficients.clear();
}

}  // namespace operations_research::math_opt
