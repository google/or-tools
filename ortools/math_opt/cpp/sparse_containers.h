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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef ORTOOLS_MATH_OPT_CPP_SPARSE_CONTAINERS_H_
#define ORTOOLS_MATH_OPT_CPP_SPARSE_CONTAINERS_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/cpp/basis_status.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/objective.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// Returns the VariableMap<double> equivalent to `vars_proto`.
//
// Requires that (or returns a status error):
//  * vars_proto.ids and vars_proto.values have equal size.
//  * vars_proto.ids is sorted.
//  * vars_proto.ids has elements that are variables in `model` (this implies
//    that each id is in [0, max(int64_t))).
//
// Note that the values of vars_proto.values are not checked (it may have NaNs).
absl::StatusOr<VariableMap<double>> VariableValuesFromProto(
    ModelStorageCPtr model, const SparseDoubleVectorProto& vars_proto);

// Returns the VariableMap<int32_t> equivalent to `vars_proto`.
//
// Requires that (or returns a status error):
//  * vars_proto.ids and vars_proto.values have equal size.
//  * vars_proto.ids is sorted.
//  * vars_proto.ids has elements that are variables in `model` (this implies
//    that each id is in [0, max(int64_t))).
absl::StatusOr<VariableMap<int32_t>> VariableValuesFromProto(
    ModelStorageCPtr model, const SparseInt32VectorProto& vars_proto);

// Returns the proto equivalent of variable_values.
SparseDoubleVectorProto VariableValuesToProto(
    const VariableMap<double>& variable_values);

// Returns an absl::flat_hash_map<Objective, double> equivalent to
// `aux_obj_proto`.
//
// Requires that (or returns a status error):
//  * The keys of `aux_obj_proto` correspond to objectives in `model`.
//
// Note that the values of `aux_obj_proto` are not checked (it may have NaNs).
absl::StatusOr<absl::flat_hash_map<Objective, double>>
AuxiliaryObjectiveValuesFromProto(
    ModelStorageCPtr model,
    const google::protobuf::Map<int64_t, double>& aux_obj_proto);

// Returns the proto equivalent of auxiliary_obj_values.
//
// Requires that (or will CHECK-fail):
//  * The keys of `aux_obj_values` all correspond to auxiliary objectives.
google::protobuf::Map<int64_t, double> AuxiliaryObjectiveValuesToProto(
    const absl::flat_hash_map<Objective, double>& aux_obj_values);

// Returns the LinearConstraintMap<double> equivalent to `lin_cons_proto`.
//
// Requires that (or returns a status error):
//  * lin_cons_proto.ids and lin_cons_proto.values have equal size.
//  * lin_cons_proto.ids is sorted.
//  * lin_cons_proto.ids has elements that are linear constraints in `model`
//    (this implies that each id is in [0, max(int64_t))).
//
// Note that the values of lin_cons_proto.values are not checked (it may have
// NaNs).
absl::StatusOr<LinearConstraintMap<double>> LinearConstraintValuesFromProto(
    ModelStorageCPtr model, const SparseDoubleVectorProto& lin_cons_proto);

// Returns the proto equivalent of linear_constraint_values.
SparseDoubleVectorProto LinearConstraintValuesToProto(
    const LinearConstraintMap<double>& linear_constraint_values);

// Returns the absl::flat_hash_map<QuadraticConstraint, double> equivalent to
// `quad_cons_proto`.
//
// Requires that (or returns a status error):
//  * quad_cons_proto.ids and quad_cons_proto.values have equal size.
//  * quad_cons_proto.ids is sorted.
//  * quad_cons_proto.ids has elements that are quadratic constraints in `model`
//    (this implies that each id is in [0, max(int64_t))).
//
// Note that the values of quad_cons_proto.values are not checked (it may have
// NaNs).
absl::StatusOr<absl::flat_hash_map<QuadraticConstraint, double>>
QuadraticConstraintValuesFromProto(
    ModelStorageCPtr model, const SparseDoubleVectorProto& quad_cons_proto);

// Returns the proto equivalent of quadratic_constraint_values.
SparseDoubleVectorProto QuadraticConstraintValuesToProto(
    const absl::flat_hash_map<QuadraticConstraint, double>&
        quadratic_constraint_values);

// Returns the VariableMap<BasisStatus> equivalent to `basis_proto`.
//
// Requires that (or returns a status error):
//  * basis_proto.ids and basis_proto.values have equal size.
//  * basis_proto.ids is sorted.
//  * basis_proto.ids has elements that are variables in `model` (this implies
//    that each id is in [0, max(int64_t))).
//  * basis_proto.values does not contain UNSPECIFIED and has valid enum values.
absl::StatusOr<VariableMap<BasisStatus>> VariableBasisFromProto(
    ModelStorageCPtr model, const SparseBasisStatusVector& basis_proto);

// Returns the proto equivalent of basis_values.
SparseBasisStatusVector VariableBasisToProto(
    const VariableMap<BasisStatus>& basis_values);

// Returns the LinearConstraintMap<BasisStatus> equivalent to `basis_proto`.
//
// Requires that (or returns a status error):
//  * basis_proto.ids and basis_proto.values have equal size.
//  * basis_proto.ids is sorted.
//  * basis_proto.ids has elements that are linear constraints in `model` (this
//    implies that each id is in [0, max(int64_t))).
//  * basis_proto.values does not contain UNSPECIFIED and has valid enum values.
absl::StatusOr<LinearConstraintMap<BasisStatus>> LinearConstraintBasisFromProto(
    ModelStorageCPtr model, const SparseBasisStatusVector& basis_proto);

// Returns the proto equivalent of basis_values.
SparseBasisStatusVector LinearConstraintBasisToProto(
    const LinearConstraintMap<BasisStatus>& basis_values);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_SPARSE_CONTAINERS_H_
