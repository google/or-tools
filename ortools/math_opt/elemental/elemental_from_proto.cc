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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/map.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/model_validator.h"

namespace operations_research::math_opt {
namespace {

absl::string_view SafeName(
    const google::protobuf::RepeatedPtrField<std::string>& names,
    const int index) {
  if (index >= names.size() || index < 0) {
    return "";
  }
  return names[index];
}

template <ElementType e>
ElementId<e> SafeAddElement(const int64_t id, const absl::string_view name,
                            Elemental& elemental) {
  elemental.EnsureNextElementIdAtLeastUntyped(e, id);
  return elemental.AddElement<e>(name);
}

template <typename T>
std::vector<std::pair<int64_t, const T*>> SortMapByKey(
    const google::protobuf::Map<int64_t, T>& proto_map) {
  std::vector<std::pair<int64_t, const T*>> result;
  result.reserve(proto_map.size());
  for (const auto& [id, val] : proto_map) {
    result.push_back({id, &val});
  }
  absl::c_sort(result);
  return result;
}

void AddVariables(const VariablesProto& variables, Elemental& elemental) {
  for (int i = 0; i < variables.ids_size(); ++i) {
    const VariableId id = SafeAddElement<ElementType::kVariable>(
        variables.ids(i), SafeName(variables.names(), i), elemental);
    elemental.SetAttr<Elemental::UBPolicy>(BoolAttr1::kVarInteger, AttrKey(id),
                                           variables.integers(i));
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kVarLb, AttrKey(id),
                                           variables.lower_bounds(i));
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kVarUb, AttrKey(id),
                                           variables.upper_bounds(i));
  }
}

void AddLinearConstraints(const LinearConstraintsProto& linear_constraints,
                          Elemental& elemental) {
  for (int i = 0; i < linear_constraints.ids_size(); ++i) {
    const LinearConstraintId id =
        SafeAddElement<ElementType::kLinearConstraint>(
            linear_constraints.ids(i), SafeName(linear_constraints.names(), i),
            elemental);
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kLinConLb, AttrKey(id),
                                           linear_constraints.lower_bounds(i));
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kLinConUb, AttrKey(id),
                                           linear_constraints.upper_bounds(i));
  }
}

void SetDoubleAttr1FromProto(const DoubleAttr1 attr,
                             const SparseDoubleVectorProto& vec,
                             Elemental& elemental) {
  for (int i = 0; i < vec.ids_size(); ++i) {
    elemental.SetAttr<Elemental::UBPolicy>(attr, AttrKey(vec.ids(i)),
                                           vec.values(i));
  }
}

void SetBoolAttr1FromProto(const BoolAttr1 attr,
                           const SparseBoolVectorProto& vec,
                           Elemental& elemental) {
  for (int i = 0; i < vec.ids_size(); ++i) {
    elemental.SetAttr<Elemental::UBPolicy>(attr, AttrKey(vec.ids(i)),
                                           vec.values(i));
  }
}

// Below, AttrType can be DoubleAttr2 or SymmetricDoubleAttr2.
template <typename AttrType>
void SetDoubleAttr2FromProto(const AttrType attr,
                             const SparseDoubleMatrixProto& mat,
                             Elemental& elemental) {
  static_assert(GetAttrKeySize<AttrType>() == 2,
                "Attribute must have key size two");
  static_assert(
      std::is_same_v<double, typename AttrTypeDescriptorT<AttrType>::ValueType>,
      "Attribute must be double valued");
  for (int i = 0; i < mat.row_ids_size(); ++i) {
    elemental.SetAttr<Elemental::UBPolicy>(
        attr, AttrKeyFor<AttrType>(mat.row_ids(i), mat.column_ids(i)),
        mat.coefficients(i));
  }
}

// Below, AttrType can be DoubleAttr2 or SymmetricDoubleAttr2.
template <typename AttrType>
void SetDoubleAttr2SliceFromProto(const AttrType attr, const int64_t first_id,
                                  const SparseDoubleVectorProto& slice,
                                  Elemental& elemental) {
  static_assert(GetAttrKeySize<AttrType>() == 2,
                "Attribute must have key size two");
  static_assert(
      std::is_same_v<double, typename AttrTypeDescriptorT<AttrType>::ValueType>,
      "Attribute must be double valued");
  for (int i = 0; i < slice.ids_size(); ++i) {
    elemental.SetAttr<Elemental::UBPolicy>(
        attr, AttrKey(first_id, slice.ids(i)), slice.values(i));
  }
}

// Below, AttrType can be DoubleAttr3 or SymmetricDoubleAttr3.
template <typename AttrType>
void SetDoubleAttr3SliceFromProto(const AttrType attr, const int64_t first_id,
                                  const SparseDoubleMatrixProto& slice,
                                  Elemental& elemental) {
  static_assert(GetAttrKeySize<AttrType>() == 3,
                "Attribute must have key size three");
  static_assert(
      std::is_same_v<double, typename AttrTypeDescriptorT<AttrType>::ValueType>,
      "Attribute must be double valued");
  for (int i = 0; i < slice.row_ids_size(); ++i) {
    elemental.SetAttr<Elemental::UBPolicy>(
        attr,
        AttrKeyFor<AttrType>(first_id, slice.row_ids(i), slice.column_ids(i)),
        slice.coefficients(i));
  }
}

absl::Status SetAuxiliaryObjectives(
    const google::protobuf::Map<int64_t, ObjectiveProto>& aux_objectives,
    Elemental& elemental) {
  for (const auto [proto_id, obj_ptr] : SortMapByKey(aux_objectives)) {
    const ObjectiveProto& objective = *obj_ptr;
    const AuxiliaryObjectiveId id =
        SafeAddElement<ElementType::kAuxiliaryObjective>(
            proto_id, objective.name(), elemental);
    elemental.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(id),
                      objective.maximize());
    elemental.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(id),
                      objective.offset());
    SetDoubleAttr2SliceFromProto(DoubleAttr2::kAuxObjLinCoef, id.value(),
                                 objective.linear_coefficients(), elemental);
    elemental.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(id),
                      objective.priority());
    if (objective.quadratic_coefficients().row_ids_size() > 0) {
      return util::InvalidArgumentErrorBuilder()
             << "quadratic coefficients not supported for auxiliary "
                "objectives, but found them in objective with id: "
             << id << " and name: " << objective.name();
    }
  }
  return absl::OkStatus();
}

void AddQuadraticConstraints(
    const google::protobuf::Map<int64_t, QuadraticConstraintProto>&
        quadratic_constraints,
    Elemental& elemental) {
  for (const auto [proto_id, quad_con_ptr] :
       SortMapByKey(quadratic_constraints)) {
    const QuadraticConstraintProto& quad_con = *quad_con_ptr;
    const QuadraticConstraintId id =
        SafeAddElement<ElementType::kQuadraticConstraint>(
            proto_id, quad_con.name(), elemental);
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kQuadConLb, AttrKey(id),
                                           quad_con.lower_bound());
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kQuadConUb, AttrKey(id),
                                           quad_con.upper_bound());
    SetDoubleAttr2SliceFromProto(DoubleAttr2::kQuadConLinCoef, id.value(),
                                 quad_con.linear_terms(), elemental);
    SetDoubleAttr3SliceFromProto(SymmetricDoubleAttr3::kQuadConQuadCoef,
                                 id.value(), quad_con.quadratic_terms(),
                                 elemental);
  }
}

void AddIndicatorConstraints(
    const google::protobuf::Map<int64_t, IndicatorConstraintProto>&
        indicator_constraints,
    Elemental& elemental) {
  for (const auto [proto_id, ind_con_ptr] :
       SortMapByKey(indicator_constraints)) {
    const IndicatorConstraintProto& ind_con = *ind_con_ptr;
    const IndicatorConstraintId id =
        SafeAddElement<ElementType::kIndicatorConstraint>(
            proto_id, ind_con.name(), elemental);
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kIndConLb, AttrKey(id),
                                           ind_con.lower_bound());
    elemental.SetAttr<Elemental::UBPolicy>(DoubleAttr1::kIndConUb, AttrKey(id),
                                           ind_con.upper_bound());
    SetDoubleAttr2SliceFromProto(DoubleAttr2::kIndConLinCoef, id.value(),
                                 ind_con.expression(), elemental);
    elemental.SetAttr<Elemental::UBPolicy>(BoolAttr1::kIndConActivateOnZero,
                                           AttrKey(id),
                                           ind_con.activate_on_zero());
    if (ind_con.has_indicator_id()) {
      const VariableId ind(ind_con.indicator_id());
      elemental.SetAttr<Elemental::UBPolicy>(VariableAttr1::kIndConIndicator,
                                             AttrKey(id), ind);
    }
  }
}

absl::StatusOr<Elemental> ElementalFromModelProtoImpl(const ModelProto& proto) {
  RETURN_IF_ERROR(ValidateModel(proto, /*check_names=*/false).status());
  if (proto.second_order_cone_constraints_size() > 0) {
    return absl::UnimplementedError(
        "Elemental does not support second order cone constraints yet");
  }
  if (proto.sos1_constraints_size() > 0) {
    return absl::UnimplementedError(
        "Elemental does not support sos1 constraints yet");
  }
  if (proto.sos2_constraints_size() > 0) {
    return absl::UnimplementedError(
        "Elemental does not support sos2 constraints yet");
  }
  Elemental elemental(proto.name(), proto.objective().name());
  AddVariables(proto.variables(), elemental);
  {
    const ObjectiveProto& objective = proto.objective();
    elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), objective.maximize());
    elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), objective.offset());
    SetDoubleAttr1FromProto(DoubleAttr1::kObjLinCoef,
                            objective.linear_coefficients(), elemental);
    SetDoubleAttr2FromProto(SymmetricDoubleAttr2::kObjQuadCoef,
                            objective.quadratic_coefficients(), elemental);
    elemental.SetAttr(IntAttr0::kObjPriority, AttrKey(), objective.priority());
  }
  RETURN_IF_ERROR(
      SetAuxiliaryObjectives(proto.auxiliary_objectives(), elemental));
  AddLinearConstraints(proto.linear_constraints(), elemental);
  SetDoubleAttr2FromProto(DoubleAttr2::kLinConCoef,
                          proto.linear_constraint_matrix(), elemental);
  AddQuadraticConstraints(proto.quadratic_constraints(), elemental);
  AddIndicatorConstraints(proto.indicator_constraints(), elemental);
  return elemental;
}

////////////////////////////////////////////////////////////////////////////////
// ModelUpdateProto
////////////////////////////////////////////////////////////////////////////////

IdNameBiMap& GetIdBiMip(ModelSummary& summary, const ElementType e) {
  switch (e) {
    case ElementType::kVariable:
      return summary.variables;
    case ElementType::kLinearConstraint:
      return summary.linear_constraints;
    case ElementType::kAuxiliaryObjective:
      return summary.auxiliary_objectives;
    case ElementType::kQuadraticConstraint:
      return summary.quadratic_constraints;
    case ElementType::kIndicatorConstraint:
      return summary.indicator_constraints;
  }
  LOG(FATAL) << "unreachable";
}

absl::StatusOr<ModelSummary> MakeSummary(const Elemental& elemental) {
  ModelSummary summary(/*check_names=*/false);
  summary.primary_objective_name = elemental.primary_objective_name();
  summary.maximize = elemental.GetAttr(BoolAttr0::kMaximize, {});
  for (const ElementType e : kElements) {
    IdNameBiMap& id_map = GetIdBiMip(summary, e);
    std::vector<int64_t> ids = elemental.AllElementsUntyped(e);
    absl::c_sort(ids);
    for (const int64_t id : ids) {
      ASSIGN_OR_RETURN(const absl::string_view name,
                       elemental.GetElementNameUntyped(e, id));
      RETURN_IF_ERROR(id_map.Insert(id, std::string(name)));
    }
    RETURN_IF_ERROR(id_map.SetNextFreeId(elemental.NextElementId(e)));
  }
  return summary;
}

const google::protobuf::RepeatedField<int64_t>& GetDeletedIds(
    const ElementType e, const ModelUpdateProto& update_proto) {
  switch (e) {
    case ElementType::kVariable:
      return update_proto.deleted_variable_ids();
    case ElementType::kLinearConstraint:
      return update_proto.deleted_linear_constraint_ids();
    case ElementType::kQuadraticConstraint:
      return update_proto.quadratic_constraint_updates()
          .deleted_constraint_ids();
    case ElementType::kAuxiliaryObjective:
      return update_proto.auxiliary_objectives_updates()
          .deleted_objective_ids();
    case ElementType::kIndicatorConstraint:
      return update_proto.indicator_constraint_updates()
          .deleted_constraint_ids();
  }
  LOG(FATAL) << "unreachable";
}

template <typename AtomicConstraintUpdatesProto>
bool AtomicConstraintUpdateIsEmpty(
    const AtomicConstraintUpdatesProto& message) {
  return message.deleted_constraint_ids_size() == 0 &&
         message.new_constraints().empty();
}

absl::Status ValidateModelUpdateProto(const Elemental& elemental,
                                      const ModelUpdateProto& update_proto) {
  if (!AtomicConstraintUpdateIsEmpty(
          update_proto.second_order_cone_constraint_updates())) {
    return absl::UnimplementedError(
        "Elemental does not support second order cone constraints yet");
  }
  if (!AtomicConstraintUpdateIsEmpty(update_proto.sos1_constraint_updates())) {
    return absl::UnimplementedError(
        "Elemental does not support sos1 constraints yet");
  }
  if (!AtomicConstraintUpdateIsEmpty(update_proto.sos2_constraint_updates())) {
    return absl::UnimplementedError(
        "Elemental does not support sos2 constraints yet");
  }
  ASSIGN_OR_RETURN(ModelSummary summary, MakeSummary(elemental));
  return ValidateModelUpdate(update_proto, summary);
}

// IMPORTANT: do this after adding new variables, it references old and new.
void ApplyObjectiveUpdates(const ObjectiveUpdatesProto& objective_updates,
                           Elemental& elemental) {
  if (objective_updates.has_direction_update()) {
    elemental.SetAttr(BoolAttr0::kMaximize, {},
                      objective_updates.direction_update());
  }
  if (objective_updates.has_offset_update()) {
    elemental.SetAttr(DoubleAttr0::kObjOffset, {},
                      objective_updates.offset_update());
  }
  if (objective_updates.has_priority_update()) {
    elemental.SetAttr(IntAttr0::kObjPriority, {},
                      objective_updates.priority_update());
  }
  SetDoubleAttr1FromProto(DoubleAttr1::kObjLinCoef,
                          objective_updates.linear_coefficients(), elemental);
  SetDoubleAttr2FromProto(SymmetricDoubleAttr2::kObjQuadCoef,
                          objective_updates.quadratic_coefficients(),
                          elemental);
}

// IMPORTANT: do this after adding new variables, it references old and new.
absl::Status ApplyAuxiliaryObjectiveUpdates(
    const ObjectiveUpdatesProto& objective_updates, const int64_t aux_obj_id,
    Elemental& elemental) {
  if (objective_updates.quadratic_coefficients().row_ids_size() > 0) {
    return absl::InvalidArgumentError(
        "quadratic coefficients are not supported for auxiliary objectives");
  }
  if (objective_updates.has_direction_update()) {
    elemental.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(aux_obj_id),
                      objective_updates.direction_update());
  }
  if (objective_updates.has_offset_update()) {
    elemental.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(aux_obj_id),
                      objective_updates.offset_update());
  }
  if (objective_updates.has_priority_update()) {
    elemental.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(aux_obj_id),
                      objective_updates.priority_update());
  }
  SetDoubleAttr2SliceFromProto(DoubleAttr2::kAuxObjLinCoef, aux_obj_id,
                               objective_updates.linear_coefficients(),
                               elemental);
  return absl::OkStatus();
}

absl::Status ElementalApplyUpdateProto(const ModelUpdateProto& update_proto,
                                       Elemental& elemental) {
  RETURN_IF_ERROR(ValidateModelUpdateProto(elemental, update_proto));
  for (const ElementType e : kElements) {
    for (const int64_t id : GetDeletedIds(e, update_proto)) {
      elemental.DeleteElementUntyped(e, id);
    }
  }

  // Update variables.
  SetDoubleAttr1FromProto(DoubleAttr1::kVarLb,
                          update_proto.variable_updates().lower_bounds(),
                          elemental);
  SetDoubleAttr1FromProto(DoubleAttr1::kVarUb,
                          update_proto.variable_updates().upper_bounds(),
                          elemental);
  SetBoolAttr1FromProto(BoolAttr1::kVarInteger,
                        update_proto.variable_updates().integers(), elemental);
  // Add new variables.
  AddVariables(update_proto.new_variables(), elemental);

  // Update the objectives. IMPORTANT: do this after adding new variables.
  ApplyObjectiveUpdates(update_proto.objective_updates(), elemental);
  for (const auto& [id, aux_obj_update] :
       update_proto.auxiliary_objectives_updates().objective_updates()) {
    RETURN_IF_ERROR(
        ApplyAuxiliaryObjectiveUpdates(aux_obj_update, id, elemental));
  }
  RETURN_IF_ERROR(SetAuxiliaryObjectives(
      update_proto.auxiliary_objectives_updates().new_objectives(), elemental));

  // Update linear constraints.
  SetDoubleAttr1FromProto(
      DoubleAttr1::kLinConLb,
      update_proto.linear_constraint_updates().lower_bounds(), elemental);
  SetDoubleAttr1FromProto(
      DoubleAttr1::kLinConUb,
      update_proto.linear_constraint_updates().upper_bounds(), elemental);
  // Add linear constraints.
  AddLinearConstraints(update_proto.new_linear_constraints(), elemental);
  // Update linear constraint matrix. IMPORTANT: do this after adding both
  // new variables and new linear constraints.
  SetDoubleAttr2FromProto(DoubleAttr2::kLinConCoef,
                          update_proto.linear_constraint_matrix_updates(),
                          elemental);

  // Quadratic constraints.
  AddQuadraticConstraints(
      update_proto.quadratic_constraint_updates().new_constraints(), elemental);
  AddIndicatorConstraints(
      update_proto.indicator_constraint_updates().new_constraints(), elemental);
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<Elemental> Elemental::FromModelProto(const ModelProto& proto) {
  // It intentional that that this function is implemented without access to the
  // private API of elemental. This allows us to change the implementation
  // elemental without breaking the proto export code.
  return ElementalFromModelProtoImpl(proto);
}

absl::Status Elemental::ApplyUpdateProto(const ModelUpdateProto& update_proto) {
  return ElementalApplyUpdateProto(update_proto, *this);
}

}  // namespace operations_research::math_opt
