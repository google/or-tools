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

#include "ortools/math_opt/constraints/quadratic/storage.h"

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

QuadraticConstraintData QuadraticConstraintData::FromProto(
    const ProtoType& in_proto) {
  QuadraticConstraintData data;
  data.lower_bound = in_proto.lower_bound();
  data.upper_bound = in_proto.upper_bound();
  data.name = in_proto.name();
  for (int i = 0; i < in_proto.linear_terms().ids_size(); ++i) {
    data.linear_terms.set(VariableId(in_proto.linear_terms().ids(i)),
                          in_proto.linear_terms().values(i));
  }
  for (int i = 0; i < in_proto.quadratic_terms().row_ids_size(); ++i) {
    data.quadratic_terms.set(
        VariableId(in_proto.quadratic_terms().row_ids(i)),
        VariableId(in_proto.quadratic_terms().column_ids(i)),
        in_proto.quadratic_terms().coefficients(i));
  }
  return data;
}

typename QuadraticConstraintData::ProtoType QuadraticConstraintData::Proto()
    const {
  ProtoType constraint;
  constraint.set_lower_bound(lower_bound);
  constraint.set_upper_bound(upper_bound);
  *constraint.mutable_linear_terms() = linear_terms.Proto();
  *constraint.mutable_quadratic_terms() = quadratic_terms.Proto();
  constraint.set_name(name);
  return constraint;
}

std::vector<VariableId> QuadraticConstraintData::RelatedVariables() const {
  std::vector<VariableId> quad_terms = quadratic_terms.Variables();
  absl::flat_hash_set<VariableId> vars(quad_terms.begin(), quad_terms.end());
  for (const auto& [var, coef] : linear_terms.terms()) {
    vars.insert(var);
  }
  return std::vector<VariableId>(vars.begin(), vars.end());
}

void QuadraticConstraintData::DeleteVariable(const VariableId var) {
  linear_terms.set(var, 0.0);
  quadratic_terms.Delete(var);
}

}  // namespace operations_research::math_opt
