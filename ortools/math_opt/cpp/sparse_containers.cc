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

#include "ortools/math_opt/cpp/sparse_containers.h"

#include <optional>

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
absl::StatusOr<IdMap<Key, BasisStatus>> BasisVectorFromProto(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto) {
  using IdType = typename Key::IdType;
  absl::flat_hash_map<IdType, BasisStatus> raw_map;
  raw_map.reserve(basis_proto.ids_size());
  for (const auto& [id, basis_status_proto_int] : MakeView(basis_proto)) {
    const auto basis_status_proto =
        static_cast<BasisStatusProto>(basis_status_proto_int);
    const std::optional<BasisStatus> basis_status =
        EnumFromProto(basis_status_proto);
    if (!basis_status.has_value()) {
      return util::InvalidArgumentErrorBuilder()
             << "basis status not specified for id " << id;
    }
    raw_map[IdType(id)] = *basis_status;
  }
  return IdMap<Key, BasisStatus>(model, std::move(raw_map));
}

template <typename Key>
SparseDoubleVectorProto IdMapToProto(const IdMap<Key, double>& id_map) {
  using IdType = typename Key::IdType;
  SparseDoubleVectorProto result;
  std::vector<std::pair<IdType, double>> sorted_entries(
      id_map.raw_map().begin(), id_map.raw_map().end());
  std::sort(sorted_entries.begin(), sorted_entries.end());
  for (const auto& [id, val] : sorted_entries) {
    result.add_ids(id.value());
    result.add_values(val);
  }
  return result;
}

template <typename Key>
SparseBasisStatusVector BasisIdMapToProto(
    const IdMap<Key, BasisStatus>& basis_map) {
  using IdType = typename Key::IdType;
  SparseBasisStatusVector result;
  std::vector<std::pair<IdType, BasisStatus>> sorted_entries(
      basis_map.raw_map().begin(), basis_map.raw_map().end());
  std::sort(sorted_entries.begin(), sorted_entries.end());
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

}  // namespace

absl::StatusOr<VariableMap<double>> VariableValuesFromProto(
    const ModelStorage* const model,
    const SparseDoubleVectorProto& vars_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(vars_proto));
  RETURN_IF_ERROR(VariableIdsExist(model, vars_proto.ids()));
  return VariableMap<double>(model, MakeView(vars_proto).as_map<VariableId>());
}

SparseDoubleVectorProto VariableValuesToProto(
    const VariableMap<double>& variable_values) {
  return IdMapToProto(variable_values);
}

absl::StatusOr<LinearConstraintMap<double>> LinearConstraintValuesFromProto(
    const ModelStorage* const model,
    const SparseDoubleVectorProto& lin_cons_proto) {
  RETURN_IF_ERROR(CheckSparseVectorProto(lin_cons_proto));
  RETURN_IF_ERROR(LinearConstraintIdsExist(model, lin_cons_proto.ids()));
  return LinearConstraintMap<double>(
      model, MakeView(lin_cons_proto).as_map<LinearConstraintId>());
}

SparseDoubleVectorProto LinearConstraintValuesToProto(
    const LinearConstraintMap<double>& linear_constraint_values) {
  return IdMapToProto(linear_constraint_values);
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
  return BasisIdMapToProto(basis_values);
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
  return BasisIdMapToProto(basis_values);
}

}  // namespace operations_research::math_opt
