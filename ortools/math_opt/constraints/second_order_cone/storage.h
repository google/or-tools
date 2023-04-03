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

#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_SECOND_ORDER_CONE_STORAGE_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_SECOND_ORDER_CONE_STORAGE_H_

#include <limits>
#include <string>
#include <vector>

#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

// Internal storage representation for a single second-order cone constraint.
//
// Implements the interface specified for the `ConstraintData` parameter of
// `AtomicConstraintStorage`.
struct SecondOrderConeConstraintData {
  using IdType = SecondOrderConeConstraintId;
  using ProtoType = SecondOrderConeConstraintProto;
  using UpdatesProtoType = SecondOrderConeConstraintUpdatesProto;

  // The `in_proto` must be in a valid state; see the inline comments on
  // `SecondOrderConeConstraintProto` for details.
  static SecondOrderConeConstraintData FromProto(const ProtoType& in_proto);
  ProtoType Proto() const;
  std::vector<VariableId> RelatedVariables() const;
  void DeleteVariable(VariableId var);

  LinearExpressionData upper_bound;
  std::vector<LinearExpressionData> arguments_to_norm;
  std::string name;
};

template <>
struct AtomicConstraintTraits<SecondOrderConeConstraintId> {
  using ConstraintData = SecondOrderConeConstraintData;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_SECOND_ORDER_CONE_STORAGE_H_
