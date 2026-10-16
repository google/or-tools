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

#ifndef ORTOOLS_MATH_OPT_CONSTRAINTS_SOS_STORAGE_TEST_HELPER_H_
#define ORTOOLS_MATH_OPT_CONSTRAINTS_SOS_STORAGE_TEST_HELPER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/constraints/sos/storage.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_testing.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

template <>
struct ModelStorageTestHelper<Sos1ConstraintId> {
  using ProtoType = SosConstraintProto;
  using ConstraintData =
      AtomicConstraintTraits<Sos1ConstraintId>::ConstraintData;

  static ConstraintData EmptyConstraint(const absl::string_view name) {
    return ConstraintData({}, {}, std::string(name));
  }

  static ConstraintData ConstraintOverVars(absl::Span<const VariableId> vars,
                                           const absl::string_view name) {
    std::vector<LinearExpressionData> exprs(vars.size());
    for (int i = 0; i < vars.size(); ++i) {
      exprs[i].coeffs.set(vars[i], 1.0);
    }
    return ConstraintData(exprs, {}, std::string(name));
  }

  static google::protobuf::Map<int64_t, ProtoType>& ModelProtoConstraints(
      ModelProto& model_proto) {
    return *model_proto.mutable_sos1_constraints();
  }

  static google::protobuf::RepeatedField<int64_t>& UpdateProtoDeletes(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_sos1_constraint_updates()
                ->mutable_deleted_constraint_ids();
  }

  static google::protobuf::Map<int64_t, ProtoType>& UpdateProtoCreates(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_sos1_constraint_updates()
                ->mutable_new_constraints();
  }
};

template <>
struct ModelStorageTestHelper<Sos2ConstraintId> {
  using ProtoType = SosConstraintProto;
  using ConstraintData =
      AtomicConstraintTraits<Sos2ConstraintId>::ConstraintData;

  static ConstraintData EmptyConstraint(const absl::string_view name) {
    return ConstraintData({}, {}, std::string(name));
  }

  static ConstraintData ConstraintOverVars(absl::Span<const VariableId> vars,
                                           const absl::string_view name) {
    std::vector<LinearExpressionData> exprs(vars.size());
    for (int i = 0; i < vars.size(); ++i) {
      exprs[i].coeffs.set(vars[i], 1.0);
    }
    return ConstraintData(exprs, {}, std::string(name));
  }

  static google::protobuf::Map<int64_t, ProtoType>& ModelProtoConstraints(
      ModelProto& model_proto) {
    return *model_proto.mutable_sos2_constraints();
  }

  static google::protobuf::RepeatedField<int64_t>& UpdateProtoDeletes(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_sos2_constraint_updates()
                ->mutable_deleted_constraint_ids();
  }

  static google::protobuf::Map<int64_t, ProtoType>& UpdateProtoCreates(
      ModelUpdateProto& model_update_proto) {
    return *model_update_proto.mutable_sos2_constraint_updates()
                ->mutable_new_constraints();
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CONSTRAINTS_SOS_STORAGE_TEST_HELPER_H_
