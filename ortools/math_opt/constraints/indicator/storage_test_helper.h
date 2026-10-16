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

#ifndef ORTOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_STORAGE_TEST_HELPER_H_
#define ORTOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_STORAGE_TEST_HELPER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/constraints/indicator/storage.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_testing.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

template <>
struct ModelStorageTestHelper<IndicatorConstraintId> {
  using ProtoType = IndicatorConstraintProto;

  static IndicatorConstraintData EmptyConstraint(const absl::string_view name) {
    return {.name = std::string(name)};
  }

  static IndicatorConstraintData ConstraintOverVars(
      absl::Span<const VariableId> vars, const absl::string_view name) {
    IndicatorConstraintData result{
        .lower_bound = -1.0, .upper_bound = 1.0, .name = std::string(name)};
    const size_t num_vars = vars.size();
    if (num_vars == 0) {
      return result;
    }
    result.indicator = vars[0];
    for (int i = 1; i < num_vars; ++i) {
      result.linear_terms.set(vars[i], 1.0);
    }
    return result;
  }

  static google::protobuf::Map<int64_t, ProtoType>& ModelProtoConstraints(
      ModelProto& model_proto) {
    return *model_proto.mutable_indicator_constraints();
  }

  static google::protobuf::RepeatedField<int64_t>& UpdateProtoDeletes(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_indicator_constraint_updates()
                ->mutable_deleted_constraint_ids();
  }

  static google::protobuf::Map<int64_t, ProtoType>& UpdateProtoCreates(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_indicator_constraint_updates()
                ->mutable_new_constraints();
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_STORAGE_TEST_HELPER_H_
