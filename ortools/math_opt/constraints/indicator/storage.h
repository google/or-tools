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

#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_STORAGE_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_STORAGE_H_

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

// Internal storage representation for a single indicator constraint.
//
// Implements the interface specified for the `ConstraintData` parameter of
// `AtomicConstraintStorage`.
struct IndicatorConstraintData {
  using IdType = IndicatorConstraintId;
  using ProtoType = IndicatorConstraintProto;
  using UpdatesProtoType = IndicatorConstraintUpdatesProto;

  // The `in_proto` must be in a valid state; see the inline comments on
  // `IndicatorConstraintProto` for details.
  static IndicatorConstraintData FromProto(const ProtoType& in_proto);
  ProtoType Proto() const;
  std::vector<VariableId> RelatedVariables() const;
  void DeleteVariable(VariableId var);

  double lower_bound = -std::numeric_limits<double>::infinity();
  double upper_bound = std::numeric_limits<double>::infinity();
  SparseCoefficientMap linear_terms;
  // The indicator variable may be unset, in which case the constraint is
  // ignored.
  std::optional<VariableId> indicator;
  bool activate_on_zero = false;
  std::string name;
};

template <>
struct AtomicConstraintTraits<IndicatorConstraintId> {
  using ConstraintData = IndicatorConstraintData;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_STORAGE_H_
