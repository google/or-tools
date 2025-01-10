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

#include "ortools/math_opt/cpp/sparse_containers.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "google/protobuf/map.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/basis_status.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/objective.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research::math_opt {
namespace {

// SparseVectorProtoType should be SparseDoubleVector or SparseBasisStatusVector
template <typename SparseVectorProtoType>
absl::Status CheckSparseVectorProto(const SparseVectorProtoType& vec) {
  RETURN_IF_ERROR(CheckIdsAndValuesSize(MakeView(vec)));
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(vec.ids()));
  return absl::OkStatus();
}

template <typename Key>
absl::StatusOr<absl::flat_hash_map<Key, BasisStatus>> BasisVectorFromProto(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto) {
  using IdType = typename Key::IdType;
  absl::flat_hash_map<Key, BasisStatus> map;
  map.reserve(basis_proto.ids_size());
  for (const auto& [id, basis_status_proto_int] : MakeView(basis_proto)) {
    const auto basis_status_proto =
        static_cast<BasisStatusProto>(basis_status_proto_int);
    const std::optional<BasisStatus> basis_status =
        EnumFromProto(basis_status_proto);
    if (!basis_status.has_value()) {
      return util::InvalidArgumentErrorBuilder()
             << "basis status not specified for id " << id;
    }
    map[Key(model, IdType(id))] = *basis_status;
  }
  return map;
}

template <typename Key>
SparseDoubleVectorProto MapToProto(
    const absl::flat_hash_map<Key, double>& id_map) {
  using IdType = typename Key::IdType;
  std::vector<std::pair<IdType, double>> sorted_entries;
  sorted_entries.reserve(id_map.size());
  for (const auto& [k, v] : id_map) {
    sorted_entries.emplace_back(k.typed_id(), v);
  }
  absl::c_sort(sorted_entries);
  SparseDoubleVectorProto result;
  for (const auto& [id, val] : sorted_entries) {
    result.add_ids(id.value());
    result.add_values(val);
  }
  return result;
}

template <typename Key>
SparseBasisStatusVector BasisMapToProto(
    const absl::flat_hash_map<Key, BasisStatus>& basis_map) {
  using IdType = typename Key::IdType;
  std::vector<std::pair<IdType, BasisStatus>> sorted_entries;
  sorted_entries.reserve(basis_map.size());
  for (const auto& [k, v] : basis_map) {
    sorted_entries.emplace_back(k.typed_id(), v);
  }
  absl::c_sort(sorted_entries);
  SparseBasisStatusVector result;
  for (const auto& [id, val] : sorted_entries) {
    result.add_ids(id.value());
    result.add_values(EnumToProto(val));
  }
  return result;
}

absl::Status VariableIdsExist(const ModelStorage* const model,
                              const absl::Span<const int64_t> ids) {
  for (const int64_t id : ids) {
    if (!model->has_variable(VariableId(id))) {
      return util::InvalidArgumentErrorBuilder()
             << "no variable with id " << id << " exists";
    }
  }
  return absl::OkStatus();
}

absl::Status LinearConstraintIdsExist(const ModelStorage* const model,
                                      const absl::Span<const int64_t> ids) {
  for (const int64_t id : ids) {
    if (!model->has_linear_constraint(LinearConstraintId(id))) {
      return util::InvalidArgumentErrorBuilder()
             << "no linear constraint with id " << id << " exists";
    }
  }
  return absl::OkStatus();
}

absl::Status QuadraticConstraintIdsExist(const ModelStorage* const model,
                                         const absl::Span<const int64_t> ids) {
  for (const int64_t id : ids) {
    if (!model->has_constraint(QuadraticConstraintId(id))) {
      return util::InvalidArgumentErrorBuilder()
             << "no quadratic constraint with id " << id << " exists";
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<VariableMap<double>> VariableValuesFromProto(
    const ModelStorage* const model,
    const SparseDoubleVectorProto& vars_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(vars_proto));
  RETURN_IF_ERROR(VariableIdsExist(model, vars_proto.ids()));
  return MakeView(vars_proto).as_map<Variable>(model);
}

absl::StatusOr<VariableMap<int32_t>> VariableValuesFromProto(
    const ModelStorage* model, const SparseInt32VectorProto& vars_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(vars_proto));
  RETURN_IF_ERROR(VariableIdsExist(model, vars_proto.ids()));
  return MakeView(vars_proto).as_map<Variable>(model);
}

SparseDoubleVectorProto VariableValuesToProto(
    const VariableMap<double>& variable_values) {
  return MapToProto(variable_values);
}

absl::StatusOr<absl::flat_hash_map<Objective, double>>
AuxiliaryObjectiveValuesFromProto(
    const ModelStorage* const model,
    const google::protobuf::Map<int64_t, double>& aux_obj_proto) {
  absl::flat_hash_map<Objective, double> result;
  for (const auto [raw_id, value] : aux_obj_proto) {
    const AuxiliaryObjectiveId id(raw_id);
    if (!model->has_auxiliary_objective(id)) {
      return util::InvalidArgumentErrorBuilder()
             << "no auxiliary objective with id " << raw_id << " exists";
    }
    result[Objective::Auxiliary(model, id)] = value;
  }
  return result;
}

google::protobuf::Map<int64_t, double> AuxiliaryObjectiveValuesToProto(
    const absl::flat_hash_map<Objective, double>& aux_obj_values) {
  google::protobuf::Map<int64_t, double> result;
  for (const auto& [objective, value] : aux_obj_values) {
    CHECK(objective.id().has_value())
        << "encountered primary objective in auxiliary objective value map";
    result[objective.id().value()] = value;
  }
  return result;
}

absl::StatusOr<LinearConstraintMap<double>> LinearConstraintValuesFromProto(
    const ModelStorage* const model,
    const SparseDoubleVectorProto& lin_cons_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(lin_cons_proto));
  RETURN_IF_ERROR(LinearConstraintIdsExist(model, lin_cons_proto.ids()));
  return MakeView(lin_cons_proto).as_map<LinearConstraint>(model);
}

SparseDoubleVectorProto LinearConstraintValuesToProto(
    const LinearConstraintMap<double>& linear_constraint_values) {
  return MapToProto(linear_constraint_values);
}

absl::StatusOr<absl::flat_hash_map<QuadraticConstraint, double>>
QuadraticConstraintValuesFromProto(
    const ModelStorage* const model,
    const SparseDoubleVectorProto& quad_cons_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(quad_cons_proto));
  RETURN_IF_ERROR(QuadraticConstraintIdsExist(model, quad_cons_proto.ids()));
  return MakeView(quad_cons_proto).as_map<QuadraticConstraint>(model);
}

SparseDoubleVectorProto QuadraticConstraintValuesToProto(
    const absl::flat_hash_map<QuadraticConstraint, double>&
        quadratic_constraint_values) {
  return MapToProto(quadratic_constraint_values);
}

absl::StatusOr<VariableMap<BasisStatus>> VariableBasisFromProto(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(basis_proto));
  RETURN_IF_ERROR(VariableIdsExist(model, basis_proto.ids()));
  return BasisVectorFromProto<Variable>(model, basis_proto);
}

SparseBasisStatusVector VariableBasisToProto(
    const VariableMap<BasisStatus>& basis_values) {
  return BasisMapToProto(basis_values);
}

absl::StatusOr<LinearConstraintMap<BasisStatus>> LinearConstraintBasisFromProto(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(basis_proto));
  RETURN_IF_ERROR(LinearConstraintIdsExist(model, basis_proto.ids()));
  return BasisVectorFromProto<LinearConstraint>(model, basis_proto);
}

SparseBasisStatusVector LinearConstraintBasisToProto(
    const LinearConstraintMap<BasisStatus>& basis_values) {
  return BasisMapToProto(basis_values);
}

}  // namespace operations_research::math_opt
