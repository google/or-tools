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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_SPARSE_CONTAINERS_H_
#define OR_TOOLS_MATH_OPT_CPP_SPARSE_CONTAINERS_H_

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/basis_status.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt {

// Returns the VariableMap<double> equivalent to `vars_proto`.
//
// Requires that (or returns a status error):
//  * vars_proto.ids and vars_proto.values have equal size.
//  * vars_proto.ids is sorted.
//  * vars_proto.ids has elements in [0, max(int64_t)).
//  * vars_proto.ids has elements that are variables in `model`.
//
// Note that the values of vars_proto.values are not checked (it may have NaNs).
absl::StatusOr<VariableMap<double>> VariableValuesFromProto(
    const ModelStorage* const model, const SparseDoubleVectorProto& vars_proto);

// Returns the proto equivalent of variable_values.
SparseDoubleVectorProto VariableValuesToProto(
    const VariableMap<double>& variable_values);

// Returns the LinearConstraintMap<double> equivalent to `lin_cons_proto`.
//
// Requires that (or returns a status error):
//  * lin_cons_proto.ids and lin_cons_proto.values have equal size.
//  * lin_cons_proto.ids is sorted.
//  * lin_cons_proto.ids has elements in [0, max(int64_t)).
//  * lin_cons_proto.ids has elements that are linear constraints in `model`.
//
// Note that the values of lin_cons_proto.values are not checked (it may have
// NaNs).
absl::StatusOr<LinearConstraintMap<double>> LinearConstraintValuesFromProto(
    const ModelStorage* const model,
    const SparseDoubleVectorProto& lin_cons_proto);

// Returns the proto equivalent of linear_constraint_values.
SparseDoubleVectorProto LinearConstraintValuesToProto(
    const LinearConstraintMap<double>& linear_constraint_values);

// Returns the VariableMap<BasisStatus> equivalent to `basis_proto`.
//
// Requires that (or returns a status error):
//  * basis_proto.ids and basis_proto.values have equal size.
//  * basis_proto.ids is sorted.
//  * basis_proto.ids has elements in [0, max(int64_t)).
//  * basis_proto.ids has elements that are variables in `model`.
//  * basis_proto.values does not contain UNSPECIFIED and has valid enum values.
absl::StatusOr<VariableMap<BasisStatus>> VariableBasisFromProto(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto);

// Returns the proto equivalent of basis_values.
SparseBasisStatusVector VariableBasisToProto(
    const VariableMap<BasisStatus>& basis_values);

// Returns the LinearConstraintMap<BasisStatus> equivalent to `basis_proto`.
//
// Requires that (or returns a status error):
//  * basis_proto.ids and basis_proto.values have equal size.
//  * basis_proto.ids is sorted.
//  * basis_proto.ids has elements in [0, max(int64_t)).
//  * basis_proto.ids has elements that are linear constraints in `model`.
//  * basis_proto.values does not contain UNSPECIFIED and has valid enum values.
absl::StatusOr<LinearConstraintMap<BasisStatus>> LinearConstraintBasisFromProto(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto);

// Returns the proto equivalent of basis_values.
SparseBasisStatusVector LinearConstraintBasisToProto(
    const LinearConstraintMap<BasisStatus>& basis_values);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_SPARSE_CONTAINERS_H_
