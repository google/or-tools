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

#include "ortools/math_opt/constraints/second_order_cone/storage.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

SecondOrderConeConstraintData SecondOrderConeConstraintData::FromProto(
    const ProtoType& in_proto) {
  SecondOrderConeConstraintData data;
  data.upper_bound = LinearExpressionData::FromProto(in_proto.upper_bound());
  data.arguments_to_norm.reserve(in_proto.arguments_to_norm_size());
  for (const LinearExpressionProto& expr_proto : in_proto.arguments_to_norm()) {
    data.arguments_to_norm.push_back(
        LinearExpressionData::FromProto(expr_proto));
  }
  data.name = in_proto.name();
  return data;
}

typename SecondOrderConeConstraintData::ProtoType
SecondOrderConeConstraintData::Proto() const {
  ProtoType constraint;
  *constraint.mutable_upper_bound() = upper_bound.Proto();
  for (const LinearExpressionData& expr : arguments_to_norm) {
    *constraint.add_arguments_to_norm() = expr.Proto();
  }
  constraint.set_name(name);
  return constraint;
}

std::vector<VariableId> SecondOrderConeConstraintData::RelatedVariables()
    const {
  absl::flat_hash_set<VariableId> vars;
  for (const auto& [var, unused] : upper_bound.coeffs.terms()) {
    vars.insert(var);
  }
  for (const LinearExpressionData& expr : arguments_to_norm) {
    for (const auto& [var, unused] : expr.coeffs.terms()) {
      vars.insert(var);
    }
  }
  return std::vector<VariableId>(vars.begin(), vars.end());
}

void SecondOrderConeConstraintData::DeleteVariable(const VariableId var) {
  upper_bound.coeffs.set(var, 0.0);
  for (LinearExpressionData& expr : arguments_to_norm) {
    expr.coeffs.set(var, 0.0);
  }
}

}  // namespace operations_research::math_opt
