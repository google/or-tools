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

#ifndef ORTOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_STORAGE_TEST_HELPER_H_
#define ORTOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_STORAGE_TEST_HELPER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/constraints/quadratic/storage.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_testing.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

template <>
struct ModelStorageTestHelper<QuadraticConstraintId> {
  using ProtoType = QuadraticConstraintProto;

  static QuadraticConstraintData EmptyConstraint(const absl::string_view name) {
    return {.name = std::string(name)};
  }

  static QuadraticConstraintData ConstraintOverVars(
      absl::Span<const VariableId> vars, const absl::string_view name) {
    QuadraticConstraintData result{.lower_bound = 0.0,
                                   .name = std::string(name)};
    for (const VariableId v : vars) {
      result.linear_terms.set(v, 1.0);
      result.quadratic_terms.set(v, v, 1.0);
    }
    return result;
  }

  static google::protobuf::Map<int64_t, ProtoType>& ModelProtoConstraints(
      ModelProto& model_proto) {
    return *model_proto.mutable_quadratic_constraints();
  }

  static google::protobuf::RepeatedField<int64_t>& UpdateProtoDeletes(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_quadratic_constraint_updates()
                ->mutable_deleted_constraint_ids();
  }

  static google::protobuf::Map<int64_t, ProtoType>& UpdateProtoCreates(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_quadratic_constraint_updates()
                ->mutable_new_constraints();
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_STORAGE_TEST_HELPER_H_
