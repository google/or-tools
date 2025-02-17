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

#include "ortools/math_opt/cpp/map_filter.h"

#include <cstdint>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research::math_opt {

absl::StatusOr<MapFilter<Variable>> VariableFilterFromProto(
    const Model& model, const SparseVectorFilterProto& proto) {
  MapFilter<Variable> result = {.skip_zero_values = proto.skip_zero_values()};
  if (proto.filter_by_ids()) {
    absl::flat_hash_set<Variable> filtered;
    for (const int64_t id : proto.filtered_ids()) {
      if (!model.has_variable(id)) {
        return util::InvalidArgumentErrorBuilder()
               << "cannot create MapFilter<Variable> from proto, variable id: "
               << id << " not in model";
      }
      filtered.insert(model.variable(id));
    }
    result.filtered_keys = std::move(filtered);
  }
  return result;
}

absl::StatusOr<MapFilter<LinearConstraint>> LinearConstraintFilterFromProto(
    const Model& model, const SparseVectorFilterProto& proto) {
  MapFilter<LinearConstraint> result = {.skip_zero_values =
                                            proto.skip_zero_values()};
  if (proto.filter_by_ids()) {
    absl::flat_hash_set<LinearConstraint> filtered;
    for (const int64_t id : proto.filtered_ids()) {
      if (!model.has_linear_constraint(id)) {
        return util::InvalidArgumentErrorBuilder()
               << "cannot create MapFilter<LinearConstraint> from proto, "
                  "linear constraint id: "
               << id << " not in model";
      }
      filtered.insert(model.linear_constraint(id));
    }
    result.filtered_keys = std::move(filtered);
  }
  return result;
}

absl::StatusOr<MapFilter<QuadraticConstraint>>
QuadraticConstraintFilterFromProto(const Model& model,
                                   const SparseVectorFilterProto& proto) {
  MapFilter<QuadraticConstraint> result = {.skip_zero_values =
                                               proto.skip_zero_values()};
  if (proto.filter_by_ids()) {
    absl::flat_hash_set<QuadraticConstraint> filtered;
    for (const int64_t id : proto.filtered_ids()) {
      if (!model.has_quadratic_constraint(id)) {
        return util::InvalidArgumentErrorBuilder()
               << "cannot create MapFilter<QuadraticConstraint> from proto, "
                  "quadratic constraint id: "
               << id << " not in model";
      }
      filtered.insert(model.quadratic_constraint(id));
    }
    result.filtered_keys = std::move(filtered);
  }
  return result;
}

}  // namespace operations_research::math_opt
