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

#include "ortools/math_opt/constraints/indicator/storage.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

IndicatorConstraintData IndicatorConstraintData::FromProto(
    const ProtoType& in_proto) {
  IndicatorConstraintData data;
  data.lower_bound = in_proto.lower_bound();
  data.upper_bound = in_proto.upper_bound();
  data.name = in_proto.name();
  data.activate_on_zero = in_proto.activate_on_zero();
  if (in_proto.has_indicator_id()) {
    data.indicator = VariableId(in_proto.indicator_id());
  }
  for (int i = 0; i < in_proto.expression().ids_size(); ++i) {
    data.linear_terms.set(VariableId(in_proto.expression().ids(i)),
                          in_proto.expression().values(i));
  }
  return data;
}

typename IndicatorConstraintData::ProtoType IndicatorConstraintData::Proto()
    const {
  ProtoType constraint;
  constraint.set_lower_bound(lower_bound);
  constraint.set_upper_bound(upper_bound);
  constraint.set_name(name);
  constraint.set_activate_on_zero(activate_on_zero);
  if (indicator.has_value()) {
    constraint.set_indicator_id(indicator->value());
  }
  for (const VariableId var : SortedMapKeys(linear_terms.terms())) {
    constraint.mutable_expression()->add_ids(var.value());
    constraint.mutable_expression()->add_values(linear_terms.get(var));
  }
  return constraint;
}

std::vector<VariableId> IndicatorConstraintData::RelatedVariables() const {
  absl::flat_hash_set<VariableId> vars;
  if (indicator.has_value()) {
    vars.insert(*indicator);
  }
  for (const auto [var, coef] : linear_terms.terms()) {
    vars.insert(var);
  }
  return std::vector<VariableId>(vars.begin(), vars.end());
}

void IndicatorConstraintData::DeleteVariable(const VariableId var) {
  linear_terms.erase(var);
  if (indicator.has_value() && *indicator == var) {
    indicator.reset();
  }
}

}  // namespace operations_research::math_opt
