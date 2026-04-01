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

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/diff.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {

namespace {

constexpr int32_t kInt32Max = std::numeric_limits<int32_t>::max();

absl::Status CanExportToProto(int64_t num_entries) {
  if (num_entries > kInt32Max) {
    return util::InvalidArgumentErrorBuilder()
           << "Cannot export to proto, RepeatedField can hold at most "
              "std::numeric_limits<int32_t>::max() = 2**31-1 = 2147483647 "
              "entries "
              "but found: "
           << num_entries << " entries";
  }
  return absl::OkStatus();
}

// Invokes fn<i>() -> Status sequentially for i in [0..n) and returns the first
// error (stopping early) or an OK status if every invocation succeeds.
//
// NOTE: move arrays.h if we need to reuse this, unit test:
// https://paste.googleplex.com/5462207396839424
template <int n, typename Fn>
absl::Status ForEachIndexUntilError(Fn&& fn) {
  absl::Status result = absl::OkStatus();
  ForEachIndex<n>([&result, &fn]<int i>() {
    if (!result.ok()) {
      return;
    }
    result.Update(fn.template operator()<i>());
  });
  return result;
}

// Applies `fn` on each value for each attribute type until an error is found,
// then returns that error, or OK if no error is found. `fn` must have a
// overload set of `operator<AttrType a>() -> Status` that accepts a `Type<i>`
// for `i` in `0..kSize`.
//
// NOTE: move derived_data.h if we need to reuse this, unit test:
// https://paste.googleplex.com/4999175629701120
template <typename Fn>
absl::Status ForEachAttrUntilError(Fn&& fn) {
  absl::Status result;
  AllAttrs::ForEachAttr([&result, &fn](auto attr) {
    if (!result.ok()) {
      return;
    }
    result.Update(fn(attr));
  });
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// ExportModelProto
////////////////////////////////////////////////////////////////////////////////

// Returns an error if there are more than 2**31-1 elements of any element type
// in `model`.
absl::Status ValidateElementsFitInProto(const Elemental& model) {
  return ForEachIndexUntilError<kNumElements>(
      [&model]<int e>() -> absl::Status {
        constexpr auto element_type = static_cast<ElementType>(e);
        RETURN_IF_ERROR(CanExportToProto(model.NumElements(element_type)))
            << "too many elements of type: " << element_type;
        return absl::OkStatus();
      });
}

// Returns an error if any attribute has more than 2**31-1 keys with a
// non-default value. We only check attributes with a key size >= 2, as we have
// already validated that the Elements fit in proto (which implies attr1s will
// fit).
absl::Status ValidateAttrsFitInProto(const Elemental& model) {
  return ForEachAttrUntilError([&model](auto attr) -> absl::Status {
    if constexpr (GetAttrKeySize<decltype(attr)>() > 1) {
      RETURN_IF_ERROR(CanExportToProto(model.AttrNumNonDefaults(attr)))
          << "too many non-default values for attribute: " << attr;
    }
    return absl::OkStatus();
  });
}

// Returns an error if model will not fit into a ModelProto.
absl::Status ValidateModelFitsInProto(const Elemental& model) {
  RETURN_IF_ERROR(ValidateElementsFitInProto(model));
  RETURN_IF_ERROR(ValidateAttrsFitInProto(model));
  return absl::OkStatus();
}

template <typename T>
std::vector<T> Sorted(std::vector<T> vec) {
  absl::c_sort(vec);
  return vec;
}

template <typename T>
std::vector<T> SortSet(const absl::flat_hash_set<T>& s) {
  return Sorted(std::vector<T>{s.begin(), s.end()});
}

template <ElementType e>
ElementIdsVector<e> Sorted(ElementIdsVector<e> vec) {
  absl::c_sort(vec.container());
  return vec;
}

// The caller must ensure that keys has at most 2**31-1 elements.
std::optional<SparseDoubleVectorProto> ExportSparseDoubleVector(
    const Elemental& elemental, DoubleAttr1 double_attr,
    absl::Span<const AttrKey<1>> keys) {
  if (keys.empty()) {
    return std::nullopt;
  }
  CHECK_LE(keys.size(), kInt32Max);
  SparseDoubleVectorProto result;
  const int32_t num_keys = static_cast<int32_t>(keys.size());
  result.mutable_ids()->Reserve(num_keys);
  result.mutable_values()->Reserve(num_keys);
  for (const AttrKey<1> key : keys) {
    result.add_ids(key[0]);
    result.add_values(elemental.GetAttr<Elemental::UBPolicy>(double_attr, key));
  }
  return result;
}

std::optional<SparseDoubleVectorProto> ExportSparseDoubleVector(
    const Elemental& elemental, DoubleAttr1 double_attr) {
  return ExportSparseDoubleVector(
      elemental, double_attr, Sorted(elemental.AttrNonDefaults(double_attr)));
}

// DAttr2 will be DoubleAttr2 or SymmetricDoubleAttr2.
//
// The caller is responsible for ensuring that there are at most 2**31-1 keys,
// otherwise UB/crash, e.g. by calling ValidateModelFitsInProto().
//
// Keys must be sorted!
template <typename DAttr2>
std::optional<SparseDoubleMatrixProto> ExportSparseDoubleMatrix(
    const Elemental& elemental, const DAttr2 attr,
    const std::vector<AttrKeyFor<DAttr2>>& keys) {
  static_assert(GetAttrKeySize<DAttr2>() == 2,
                "Attribute must have key size two");
  static_assert(
      std::is_same_v<double, typename AttrTypeDescriptorT<DAttr2>::ValueType>,
      "Attribute must be double valued");
  if (keys.empty()) {
    return std::nullopt;
  }
  CHECK_LE(keys.size(), kInt32Max);
  SparseDoubleMatrixProto result;
  // See function level comment, the caller must ensure this is safe.
  const int nnz = static_cast<int>(keys.size());
  result.mutable_row_ids()->Reserve(nnz);
  result.mutable_column_ids()->Reserve(nnz);
  result.mutable_coefficients()->Reserve(nnz);
  for (const AttrKeyFor<DAttr2> key : keys) {
    result.add_row_ids(key[0]);
    result.add_column_ids(key[1]);
    // We're using `UBPolicy` for `GetAttr` since we just obtained `keys` from
    // the model.
    result.add_coefficients(elemental.GetAttr<Elemental::UBPolicy>(attr, key));
  }
  return result;
}

// It is the caller's responsibility to ensure that the size of the slice is at
// most 2**31-1.
template <int key_index>
std::optional<SparseDoubleVectorProto> ExportSparseDoubleMatrixSlice(
    const Elemental& elemental, const DoubleAttr2 attr,
    const int64_t slice_element_id) {
  std::vector<AttrKey<2>> slice =
      elemental.Slice<key_index>(attr, slice_element_id);
  if (slice.empty()) {
    return std::nullopt;
  }
  CHECK_LE(slice.size(), kInt32Max);
  const int slice_size = static_cast<int>(slice.size());
  absl::c_sort(slice);
  SparseDoubleVectorProto vec;
  vec.mutable_ids()->Reserve(slice_size);
  vec.mutable_values()->Reserve(slice_size);
  for (const AttrKey<2> key : slice) {
    vec.add_ids(key.RemoveElement<key_index>()[0]);
    vec.add_values(elemental.GetAttr<Elemental::UBPolicy>(attr, key));
  }
  return vec;
}

template <typename DAttr2>
std::optional<SparseDoubleMatrixProto> ExportSparseDoubleMatrix(
    const Elemental& elemental, const DAttr2 attr) {
  return ExportSparseDoubleMatrix(elemental, attr,
                                  Sorted(elemental.AttrNonDefaults(attr)));
}

std::optional<VariablesProto> ExportVariables(
    const Elemental& elemental,
    const ElementIdsSpan<ElementType::kVariable> var_ids,
    const bool remove_names) {
  if (var_ids.empty()) {
    return std::nullopt;
  }
  VariablesProto vars_proto;
  // Safe because we have called ValidateModelFitsInProto().
  const int num_vars = static_cast<int>(var_ids.size());
  vars_proto.mutable_ids()->Reserve(num_vars);
  vars_proto.mutable_integers()->Reserve(num_vars);
  vars_proto.mutable_lower_bounds()->Reserve(num_vars);
  vars_proto.mutable_upper_bounds()->Reserve(num_vars);
  if (!remove_names) {
    vars_proto.mutable_names()->Reserve(num_vars);
  }
  for (const VariableId var : var_ids) {
    vars_proto.add_ids(var.value());
    // We're using `UBPolicy` for `GetAttr` since we just obtained `var_ids`
    // from the model.
    vars_proto.add_integers(elemental.GetAttr<Elemental::UBPolicy>(
        BoolAttr1::kVarInteger, AttrKey(var)));
    vars_proto.add_lower_bounds(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kVarLb, AttrKey(var)));
    vars_proto.add_upper_bounds(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kVarUb, AttrKey(var)));
    if (!remove_names) {
      const auto name = elemental.GetElementName(var);
      CHECK_OK(name);
      vars_proto.add_names(*name);
    }
  }
  return vars_proto;
}

std::optional<LinearConstraintsProto> ExportLinearConstraints(
    const Elemental& elemental,
    const ElementIdsSpan<ElementType::kLinearConstraint> lin_con_ids,
    const bool remove_names) {
  if (lin_con_ids.empty()) {
    return std::nullopt;
  }
  LinearConstraintsProto lin_cons_proto;
  // Safe because we have called ValidateModelFitsInProto().
  const int num_lin_cons = static_cast<int>(lin_con_ids.size());

  lin_cons_proto.mutable_ids()->Reserve(num_lin_cons);
  lin_cons_proto.mutable_lower_bounds()->Reserve(num_lin_cons);
  lin_cons_proto.mutable_upper_bounds()->Reserve(num_lin_cons);
  if (!remove_names) {
    lin_cons_proto.mutable_names()->Reserve(num_lin_cons);
  }
  for (const LinearConstraintId lin_con : lin_con_ids) {
    lin_cons_proto.add_ids(lin_con.value());
    // We're using `UBPolicy` for `GetAttr` since we just obtained
    // `lin_con_ids` from the model.
    lin_cons_proto.add_lower_bounds(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kLinConLb, AttrKey(lin_con)));
    lin_cons_proto.add_upper_bounds(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kLinConUb, AttrKey(lin_con)));
    if (!remove_names) {
      const auto name = elemental.GetElementName(lin_con);
      CHECK_OK(name);
      lin_cons_proto.add_names(*name);
    }
  }
  return lin_cons_proto;
}

// This function will crash if there are more than 2**31 elements in
// quad_con_ids.
absl::flat_hash_map<QuadraticConstraintId, QuadraticConstraintProto>
ExportQuadraticConstraints(
    const Elemental& elemental,
    const ElementIdsSpan<ElementType::kQuadraticConstraint> quad_con_ids,
    const bool remove_names) {
  absl::flat_hash_map<QuadraticConstraintId, QuadraticConstraintProto> result;
  CHECK_LE(quad_con_ids.size(), kInt32Max);
  for (const QuadraticConstraintId id : quad_con_ids) {
    QuadraticConstraintProto quad_con;
    if (!remove_names) {
      const auto name = elemental.GetElementName(id);
      CHECK_OK(name);
      quad_con.set_name(*name);
    }
    quad_con.set_lower_bound(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kQuadConLb, AttrKey(id)));
    quad_con.set_upper_bound(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kQuadConUb, AttrKey(id)));
    if (std::optional<SparseDoubleVectorProto> lin_coefs =
            ExportSparseDoubleMatrixSlice<0>(
                elemental, DoubleAttr2::kQuadConLinCoef, id.value());
        lin_coefs.has_value()) {
      *quad_con.mutable_linear_terms() = *std::move(lin_coefs);
    }
    if (std::vector<AttrKey<3, ElementSymmetry<1, 2>>> quad_con_quad_coefs =
            elemental.Slice<0>(SymmetricDoubleAttr3::kQuadConQuadCoef,
                               id.value());
        !quad_con_quad_coefs.empty()) {
      absl::c_sort(quad_con_quad_coefs);
      SparseDoubleMatrixProto& quad_coef_proto =
          *quad_con.mutable_quadratic_terms();
      for (const AttrKey<3, ElementSymmetry<1, 2>> key : quad_con_quad_coefs) {
        quad_coef_proto.add_row_ids(key[1]);
        quad_coef_proto.add_column_ids(key[2]);
        quad_coef_proto.add_coefficients(elemental.GetAttr<Elemental::UBPolicy>(
            SymmetricDoubleAttr3::kQuadConQuadCoef, key));
      }
    }
    auto [it, inserted] = result.insert({id, std::move(quad_con)});
    CHECK(inserted);
  }
  return result;
}

absl::flat_hash_map<IndicatorConstraintId, IndicatorConstraintProto>
ExportIndicatorConstraints(
    const Elemental& elemental,
    const ElementIdsSpan<ElementType::kIndicatorConstraint> ind_con_ids,
    const bool remove_names) {
  absl::flat_hash_map<IndicatorConstraintId, IndicatorConstraintProto> result;
  CHECK_LE(ind_con_ids.size(), kInt32Max);
  for (const IndicatorConstraintId id : ind_con_ids) {
    IndicatorConstraintProto ind_con;
    if (!remove_names) {
      const auto name = elemental.GetElementName(id);
      CHECK_OK(name);
      ind_con.set_name(*name);
    }
    ind_con.set_lower_bound(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kIndConLb, AttrKey(id)));
    ind_con.set_upper_bound(elemental.GetAttr<Elemental::UBPolicy>(
        DoubleAttr1::kIndConUb, AttrKey(id)));
    if (std::optional<SparseDoubleVectorProto> lin_coefs =
            ExportSparseDoubleMatrixSlice<0>(
                elemental, DoubleAttr2::kIndConLinCoef, id.value());
        lin_coefs.has_value()) {
      *ind_con.mutable_expression() = *std::move(lin_coefs);
    }
    ind_con.set_activate_on_zero(elemental.GetAttr<Elemental::UBPolicy>(
        BoolAttr1::kIndConActivateOnZero, AttrKey(id)));
    if (elemental.AttrIsNonDefault<Elemental::UBPolicy>(
            VariableAttr1::kIndConIndicator, AttrKey(id))) {
      ind_con.set_indicator_id(
          elemental
              .GetAttr<Elemental::UBPolicy>(VariableAttr1::kIndConIndicator,
                                            AttrKey(id))
              .value());
    }
    CHECK(result.insert({id, std::move(ind_con)}).second);
  }
  return result;
}

std::optional<ObjectiveProto> ExportObjective(const Elemental& elemental,
                                              const bool remove_names) {
  const bool has_offset =
      elemental.AttrIsNonDefault(DoubleAttr0::kObjOffset, AttrKey());
  const bool has_maximize =
      elemental.AttrIsNonDefault(BoolAttr0::kMaximize, AttrKey());
  const bool has_priority =
      elemental.AttrIsNonDefault(IntAttr0::kObjPriority, AttrKey());
  // We have less than 2**31 elements from existing validation.
  std::optional<SparseDoubleVectorProto> lin_obj_vec =
      ExportSparseDoubleVector(elemental, DoubleAttr1::kObjLinCoef);
  // We have less than 2**31 elements from existing validation.
  std::optional<SparseDoubleMatrixProto> quad_obj_mat =
      ExportSparseDoubleMatrix(elemental, SymmetricDoubleAttr2::kObjQuadCoef);
  const bool has_name =
      !remove_names && !elemental.primary_objective_name().empty();
  if (!has_offset && !has_maximize && !has_priority &&
      !lin_obj_vec.has_value() && !quad_obj_mat.has_value() && !has_name) {
    return std::nullopt;
  }
  ObjectiveProto result;
  if (!remove_names) {
    result.set_name(elemental.primary_objective_name());
  }
  result.set_maximize(elemental.GetAttr(BoolAttr0::kMaximize, AttrKey()));
  result.set_offset(elemental.GetAttr(DoubleAttr0::kObjOffset, AttrKey()));
  result.set_priority(elemental.GetAttr(IntAttr0::kObjPriority, AttrKey()));
  if (lin_obj_vec.has_value()) {
    *result.mutable_linear_coefficients() = *std::move(lin_obj_vec);
  }
  if (quad_obj_mat.has_value()) {
    *result.mutable_quadratic_coefficients() = *std::move(quad_obj_mat);
  }
  return result;
}

absl::StatusOr<ObjectiveProto> ExportAuxiliaryObjective(
    const Elemental& elemental, const AuxiliaryObjectiveId id,
    const bool remove_names) {
  ObjectiveProto result;
  if (!remove_names) {
    ASSIGN_OR_RETURN(const absl::string_view name,
                     elemental.GetElementName(id));
    result.set_name(name);
  }
  result.set_maximize(
      elemental.GetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(id)));
  result.set_offset(elemental.GetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(id)));
  result.set_priority(
      elemental.GetAttr(IntAttr1::kAuxObjPriority, AttrKey(id)));
  if (std::optional<SparseDoubleVectorProto> lin_coefs =
          ExportSparseDoubleMatrixSlice<0>(
              elemental, DoubleAttr2::kAuxObjLinCoef, id.value());
      lin_coefs.has_value()) {
    *result.mutable_linear_coefficients() = *std::move(lin_coefs);
  }
  return result;
}

absl::StatusOr<ModelProto> ExportModelProto(const Elemental& elemental,
                                            const bool remove_names) {
  RETURN_IF_ERROR(ValidateModelFitsInProto(elemental));
  ModelProto result;
  if (!remove_names) {
    result.set_name(elemental.model_name());
  }
  if (auto vars = ExportVariables(
          elemental,
          Sorted(elemental.AllElements<ElementType::kVariable>()).view(),
          remove_names);
      vars.has_value()) {
    *result.mutable_variables() = *std::move(vars);
  }

  // ObjectiveProto
  if (auto obj = ExportObjective(elemental, remove_names); obj.has_value()) {
    *result.mutable_objective() = *std::move(obj);
  }
  // Auxiliary objectives
  for (const AuxiliaryObjectiveId aux_obj_id :
       elemental.AllElements<ElementType::kAuxiliaryObjective>()) {
    ASSIGN_OR_RETURN(
        ((*result.mutable_auxiliary_objectives())[aux_obj_id.value()]),
        ExportAuxiliaryObjective(elemental, aux_obj_id, remove_names));
  }
  // LinearConstraintsProto
  if (auto lin_cons = ExportLinearConstraints(
          elemental,
          Sorted(elemental.AllElements<ElementType::kLinearConstraint>())
              .view(),
          remove_names);
      lin_cons.has_value()) {
    *result.mutable_linear_constraints() = *std::move(lin_cons);
  }

  // Linear constraint matrix proto
  if (auto mat = ExportSparseDoubleMatrix(
          elemental, DoubleAttr2::kLinConCoef,
          Sorted(elemental.AttrNonDefaults(DoubleAttr2::kLinConCoef)));
      mat.has_value()) {
    *result.mutable_linear_constraint_matrix() = *std::move(mat);
  }

  // Quadratic constraints
  for (auto& [id, quad_con_coef] : ExportQuadraticConstraints(
           elemental,
           Sorted(elemental.AllElements<ElementType::kQuadraticConstraint>())
               .view(),
           remove_names)) {
    (*result.mutable_quadratic_constraints())[id.value()] =
        std::move(quad_con_coef);
  }
  // Indicator constraints
  for (auto& [id, ind_con] : ExportIndicatorConstraints(
           elemental,
           Sorted(elemental.AllElements<ElementType::kIndicatorConstraint>())
               .view(),
           remove_names)) {
    (*result.mutable_indicator_constraints())[id.value()] = std::move(ind_con);
  }

  return result;
}

}  // namespace

absl::StatusOr<ModelProto> Elemental::ExportModel(
    const bool remove_names) const {
  // It intentional that that this function is implemented without access to the
  // private API of elemental. This allows us to change the implementation
  // elemental without breaking the proto export code.
  return ExportModelProto(*this, remove_names);
}

////////////////////////////////////////////////////////////////////////////////
// ExportModelUpdateProto
////////////////////////////////////////////////////////////////////////////////

namespace {

// Returns an error if there are more than 2**31-1 new elements or deleted
// elements of any element type.
absl::Status ValidateElementUpdatesFitInProto(
    const Diff& diff,
    const std::array<std::vector<int64_t>, kNumElements>& new_elements) {
  return ForEachIndexUntilError<kNumElements>(
      [&diff, &new_elements]<int e>() -> absl::Status {
        constexpr auto element_type = static_cast<ElementType>(e);
        RETURN_IF_ERROR(
            CanExportToProto(diff.deleted_elements(element_type).size()))
            << "too many deleted elements of type: " << element_type;
        RETURN_IF_ERROR(CanExportToProto(new_elements[e].size()))
            << "too many new elements of type: " << element_type;
        return absl::OkStatus();
      });
}

// Returns an error if the number number of tracked modifications exceeds
// 2**31-1 for any attribute.
//
// TODO(b/372411343): this is too conservative for quadratic constraints.
absl::Status ValidateAttrUpdatesFitInProto(const Diff& diff) {
  return ForEachAttrUntilError([&diff](auto attr) -> absl::Status {
    RETURN_IF_ERROR(CanExportToProto(diff.modified_keys(attr).size()))
        << "too many modifications for attribute: " << attr;
    return absl::OkStatus();
  });
}

// Checks some necessary (but not sufficient) conditions that we can build a
// ModelUpdateProto for this diff.
//
// Validates that:
//   * For each element type, we deletes at most 2**31-1 existing elements.
//   * For each element type, we add at most 2**31-1 new elements
//   * For each attribute, we update at most 2**31-1 keys on existing elements.
//
// This validation does not ensure we can actually build a ModelUpdateProto,
// further validation is required, some of which is specific to how
// ModelUpdateProto stores attributes and elements. For example:
//   * For any attribute with key size >= 2, we have not checked that the number
//     of keys containing a new element is at most 2**31-1.
//   * The linear objective coefficients and linear constraint coefficients
//     store both updates to keys on existing elements and attribute values for
//     keys containing a new elements in the same repeated field, so we need to
//     check that their combined size is at most 2**31-1.
absl::Status ValidateModelUpdateFitsInProto(
    const Diff& diff,
    const std::array<std::vector<int64_t>, kNumElements>& new_elements) {
  RETURN_IF_ERROR(ValidateElementUpdatesFitInProto(diff, new_elements));
  RETURN_IF_ERROR(ValidateAttrUpdatesFitInProto(diff));
  return absl::OkStatus();
}

// No need to return optional, repeated fields have no presence.
template <ElementType e>
google::protobuf::RepeatedField<int64_t> DeletedIdsSorted(const Diff& diff) {
  const std::vector<int64_t> sorted_del_vars =
      SortSet(diff.deleted_elements(e));
  return {sorted_del_vars.begin(), sorted_del_vars.end()};
}

std::optional<SparseDoubleVectorProto> ExportAttrDiff(
    const Elemental& elemental, DoubleAttr1 a, const Diff& diff) {
  std::vector<AttrKey<1>> keys = elemental.ModifiedKeysThatExist(a, diff);
  if (keys.empty()) {
    return std::nullopt;
  }
  absl::c_sort(keys);
  SparseDoubleVectorProto result;
  // NOTE: this cast is safe, we called ValidateModelUpdateFitsInProto().
  const int num_keys = static_cast<int>(keys.size());
  result.mutable_ids()->Reserve(num_keys);
  result.mutable_values()->Reserve(num_keys);
  for (const auto key : keys) {
    result.add_ids(key[0]);
    result.add_values(elemental.GetAttr(a, key));
  }
  return result;
}

absl::StatusOr<std::optional<SparseDoubleVectorProto>> ExportLinObjCoefUpdate(
    const Elemental& elemental, const Diff& diff,
    const ElementIdsSpan<ElementType::kVariable> new_var_ids_sorted) {
  std::vector<AttrKey<1>> keys =
      elemental.ModifiedKeysThatExist(DoubleAttr1::kObjLinCoef, diff);
  absl::c_sort(keys);
  // This is double the hashing we should be doing.
  for (const VariableId id : new_var_ids_sorted) {
    if (elemental.AttrIsNonDefault<Elemental::UBPolicy>(
            DoubleAttr1::kObjLinCoef, AttrKey(id))) {
      keys.push_back(AttrKey(id));
    }
  }
  RETURN_IF_ERROR(CanExportToProto(keys.size()))
      << "cannot export linear objective coefficients in model update";
  return ExportSparseDoubleVector(elemental, DoubleAttr1::kObjLinCoef, keys);
}

absl::StatusOr<std::optional<SparseDoubleMatrixProto>> ExportQuadObjCoefUpdate(
    const Elemental& elemental, const Diff& diff,
    const ElementIdsSpan<ElementType::kVariable> new_var_ids_sorted) {
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  std::vector<Key> keys =
      elemental.ModifiedKeysThatExist(SymmetricDoubleAttr2::kObjQuadCoef, diff);
  if (!new_var_ids_sorted.empty()) {
    const int64_t smallest_key = new_var_ids_sorted[0].value();
    // This is ~double the hashing we should be doing.
    for (const VariableId id : new_var_ids_sorted) {
      for (const Key key :
           elemental.Slice<0>(SymmetricDoubleAttr2::kObjQuadCoef, id.value())) {
        if (key[0] < smallest_key || key[1] < smallest_key ||
            key[0] == id.value()) {
          keys.push_back(key);
        }
      }
    }
  }
  RETURN_IF_ERROR(CanExportToProto(keys.size()))
      << "cannot export linear objective coefficients in model update";
  absl::c_sort(keys);
  return ExportSparseDoubleMatrix(elemental, SymmetricDoubleAttr2::kObjQuadCoef,
                                  keys);
}

std::optional<SparseBoolVectorProto> ExportAttrDiff(const Elemental& elemental,
                                                    const BoolAttr1 a,
                                                    const Diff& diff) {
  std::vector<AttrKey<1>> keys = elemental.ModifiedKeysThatExist(a, diff);
  if (keys.empty()) {
    return std::nullopt;
  }
  absl::c_sort(keys);
  SparseBoolVectorProto result;
  // NOTE: this cast is safe, we called ValidateModelUpdateFitsInProto().
  const int num_keys = static_cast<int>(keys.size());
  result.mutable_ids()->Reserve(num_keys);
  result.mutable_values()->Reserve(num_keys);
  for (const auto id : keys) {
    result.add_ids(id[0]);
    result.add_values(elemental.GetAttr(a, id));
  }
  return result;
}

std::vector<int64_t> ElementsSinceCheckpoint(const ElementType e,
                                             const Elemental& elemental,
                                             const Diff& diff) {
  std::vector<int64_t> result;
  for (int64_t id = diff.checkpoint(e); id < elemental.NextElementId(e); ++id) {
    if (elemental.ElementExistsUntyped(e, id)) {
      result.push_back(id);
    }
  }
  return result;
}

std::array<std::vector<int64_t>, kNumElements> ElementsSinceCheckpointPerType(
    const Elemental& elemental, const Diff& diff) {
  std::array<std::vector<int64_t>, kNumElements> result;
  for (const ElementType e : kElements) {
    result[static_cast<int>(e)] = ElementsSinceCheckpoint(e, elemental, diff);
  };
  return result;
}

std::optional<VariableUpdatesProto> ExportVariableUpdates(
    const Elemental& elemental, const Diff& diff) {
  auto ubs = ExportAttrDiff(elemental, DoubleAttr1::kVarUb, diff);
  auto lbs = ExportAttrDiff(elemental, DoubleAttr1::kVarLb, diff);
  auto integers = ExportAttrDiff(elemental, BoolAttr1::kVarInteger, diff);
  if (!ubs.has_value() && !lbs.has_value() && !integers.has_value()) {
    return std::nullopt;
  }
  VariableUpdatesProto var_updates;
  if (ubs.has_value()) {
    *var_updates.mutable_upper_bounds() = *std::move(ubs);
  }
  if (lbs.has_value()) {
    *var_updates.mutable_lower_bounds() = *std::move(lbs);
  }
  if (integers.has_value()) {
    *var_updates.mutable_integers() = *std::move(integers);
  }
  return var_updates;
}

std::optional<LinearConstraintUpdatesProto> ExportLinearConstraintUpdates(
    const Elemental& elemental, const Diff& diff) {
  auto ubs = ExportAttrDiff(elemental, DoubleAttr1::kLinConUb, diff);
  auto lbs = ExportAttrDiff(elemental, DoubleAttr1::kLinConLb, diff);
  if (!ubs.has_value() && !lbs.has_value()) {
    return std::nullopt;
  }
  LinearConstraintUpdatesProto lin_con_updates;
  if (ubs.has_value()) {
    *lin_con_updates.mutable_upper_bounds() = *std::move(ubs);
  }
  if (lbs.has_value()) {
    *lin_con_updates.mutable_lower_bounds() = *std::move(lbs);
  }
  return lin_con_updates;
}

absl::StatusOr<std::optional<ObjectiveUpdatesProto>> ExportObjectiveUpdates(
    const Elemental& elemental, const Diff& diff,
    const ElementIdsSpan<ElementType::kVariable> new_var_ids) {
  ASSIGN_OR_RETURN(std::optional<SparseDoubleVectorProto> lin_coef_updates,
                   ExportLinObjCoefUpdate(elemental, diff, new_var_ids));
  ASSIGN_OR_RETURN(std::optional<SparseDoubleMatrixProto> quad_coef_updates,
                   ExportQuadObjCoefUpdate(elemental, diff, new_var_ids));
  const bool maximize_modified =
      diff.modified_keys(BoolAttr0::kMaximize).contains(AttrKey());
  const bool offset_modified =
      diff.modified_keys(DoubleAttr0::kObjOffset).contains(AttrKey());
  const bool priority_modified =
      diff.modified_keys(IntAttr0::kObjPriority).contains(AttrKey());
  if (!lin_coef_updates.has_value() && !quad_coef_updates.has_value() &&
      !maximize_modified && !offset_modified && !priority_modified) {
    return std::nullopt;
  }
  ObjectiveUpdatesProto objective_updates;
  if (maximize_modified) {
    objective_updates.set_direction_update(
        elemental.GetAttr(BoolAttr0::kMaximize, AttrKey()));
  }
  if (offset_modified) {
    objective_updates.set_offset_update(
        elemental.GetAttr(DoubleAttr0::kObjOffset, AttrKey()));
  }
  if (priority_modified) {
    objective_updates.set_priority_update(
        elemental.GetAttr(IntAttr0::kObjPriority, AttrKey()));
  }
  if (lin_coef_updates.has_value()) {
    *objective_updates.mutable_linear_coefficients() =
        *std::move(lin_coef_updates);
  }
  if (quad_coef_updates.has_value()) {
    *objective_updates.mutable_quadratic_coefficients() =
        *std::move(quad_coef_updates);
  }
  return objective_updates;
}

bool ModelUpdateIsEmpty(
    const Diff& diff,
    const std::array<std::vector<int64_t>, kNumElements> new_elements) {
  for (const std::vector<int64_t>& els : new_elements) {
    if (!els.empty()) {
      return false;
    }
  }
  bool is_empty = true;
  for (const ElementType e : kElements) {
    is_empty =
        is_empty && diff.deleted_elements(static_cast<ElementType>(e)).empty();
  }
  // Subtle: we do not need to check for attribute modifications on a key
  // containing a new element, as if there is a new element, we have already
  // shown that update is non-empty.
  AllAttrs::ForEachAttr([&diff, &is_empty](auto attr) {
    is_empty = is_empty && diff.modified_keys(attr).empty();
  });
  return is_empty;
}

template <typename AttrType>
absl::Status EnsureAttrModificationsEmpty(const Diff& diff, AttrType attr) {
  if (!diff.modified_keys(attr).empty()) {
    return util::InvalidArgumentErrorBuilder()
           << "Modification for attribute " << attr
           << " is not supported for ModelUpdateProto export.";
  }
  return absl::OkStatus();
}

absl::StatusOr<std::optional<QuadraticConstraintUpdatesProto>>
ExportQuadraticConstraintsUpdates(
    const Elemental& elemental, const Diff& diff,
    const ElementIdsSpan<ElementType::kQuadraticConstraint> new_quad_cons,
    const bool remove_names) {
  // Quadratic constraints are currently immutable (beyond variable deletions)
  RETURN_IF_ERROR(EnsureAttrModificationsEmpty(diff, DoubleAttr1::kQuadConLb));
  RETURN_IF_ERROR(EnsureAttrModificationsEmpty(diff, DoubleAttr1::kQuadConUb));
  RETURN_IF_ERROR(
      EnsureAttrModificationsEmpty(diff, DoubleAttr2::kQuadConLinCoef));
  RETURN_IF_ERROR(EnsureAttrModificationsEmpty(
      diff, SymmetricDoubleAttr3::kQuadConQuadCoef));
  auto deleted = DeletedIdsSorted<ElementType::kQuadraticConstraint>(diff);
  if (deleted.empty() && new_quad_cons.empty()) {
    return std::nullopt;
  }
  QuadraticConstraintUpdatesProto result;
  *result.mutable_deleted_constraint_ids() = std::move(deleted);
  for (auto& [id, quad_con] :
       ExportQuadraticConstraints(elemental, new_quad_cons, remove_names)) {
    result.mutable_new_constraints()->insert({id.value(), std::move(quad_con)});
  }
  return result;
}

absl::StatusOr<std::optional<IndicatorConstraintUpdatesProto>>
ExportIndicatorConstraintsUpdates(
    const Elemental& elemental, const Diff& diff,
    const ElementIdsSpan<ElementType::kIndicatorConstraint> new_ind_cons,
    const bool remove_names) {
  // Indicator constraints are currently immutable (beyond variable deletions)
  RETURN_IF_ERROR(
      EnsureAttrModificationsEmpty(diff, BoolAttr1::kIndConActivateOnZero));
  RETURN_IF_ERROR(
      EnsureAttrModificationsEmpty(diff, VariableAttr1::kIndConIndicator));
  RETURN_IF_ERROR(EnsureAttrModificationsEmpty(diff, DoubleAttr1::kIndConLb));
  RETURN_IF_ERROR(EnsureAttrModificationsEmpty(diff, DoubleAttr1::kIndConUb));
  RETURN_IF_ERROR(
      EnsureAttrModificationsEmpty(diff, DoubleAttr2::kIndConLinCoef));
  auto deleted = DeletedIdsSorted<ElementType::kIndicatorConstraint>(diff);
  if (deleted.empty() && new_ind_cons.empty()) {
    return std::nullopt;
  }
  IndicatorConstraintUpdatesProto result;
  *result.mutable_deleted_constraint_ids() = std::move(deleted);
  for (auto& [id, ind_con] :
       ExportIndicatorConstraints(elemental, new_ind_cons, remove_names)) {
    result.mutable_new_constraints()->insert({id.value(), std::move(ind_con)});
  }
  return result;
}

absl::StatusOr<std::optional<AuxiliaryObjectivesUpdatesProto>>
ExportAuxiliaryObjectivesUpdates(
    const Elemental& elemental, const Diff& diff,
    const ElementIdsSpan<ElementType::kVariable> new_vars,
    const ElementIdsSpan<ElementType::kAuxiliaryObjective> new_aux_objs,
    const bool remove_names) {
  AuxiliaryObjectivesUpdatesProto result;
  auto deleted = DeletedIdsSorted<ElementType::kAuxiliaryObjective>(diff);
  // Look for modifications to existing objectives, if we have any existing
  // auxiliary objectives.
  if (diff.checkpoint(ElementType::kAuxiliaryObjective) > 0) {
    google::protobuf::Map<int64_t, ObjectiveUpdatesProto>& mods =
        *result.mutable_objective_updates();
    for (const AttrKey<1> aux_obj :
         diff.modified_keys(BoolAttr1::kAuxObjMaximize)) {
      mods[aux_obj[0]].set_direction_update(
          elemental.GetAttr<Elemental::UBPolicy>(BoolAttr1::kAuxObjMaximize,
                                                 aux_obj));
    }
    for (const AttrKey<1> aux_obj :
         diff.modified_keys(IntAttr1::kAuxObjPriority)) {
      mods[aux_obj[0]].set_priority_update(
          elemental.GetAttr<Elemental::UBPolicy>(IntAttr1::kAuxObjPriority,
                                                 aux_obj));
    }
    for (const AttrKey<1> aux_obj :
         diff.modified_keys(DoubleAttr1::kAuxObjOffset)) {
      mods[aux_obj[0]].set_offset_update(elemental.GetAttr<Elemental::UBPolicy>(
          DoubleAttr1::kAuxObjOffset, aux_obj));
    }
    absl::flat_hash_map<int64_t, std::vector<std::pair<int64_t, double>>>
        lin_con_updates;
    for (const AttrKey<2> aux_lin_obj_var :
         elemental.ModifiedKeysThatExist(DoubleAttr2::kAuxObjLinCoef, diff)) {
      lin_con_updates[aux_lin_obj_var[0]].push_back(
          {aux_lin_obj_var[1],
           elemental.GetAttr<Elemental::UBPolicy>(DoubleAttr2::kAuxObjLinCoef,
                                                  aux_lin_obj_var)});
    }
    for (const VariableId new_var : new_vars) {
      for (const AttrKey<2> aux_lin_obj_var :
           elemental.Slice<1>(DoubleAttr2::kAuxObjLinCoef, new_var.value())) {
        const int64_t aux_obj = aux_lin_obj_var[0];
        if (aux_obj >= diff.checkpoint(ElementType::kAuxiliaryObjective)) {
          continue;
        }
        lin_con_updates[aux_obj].push_back(
            {new_var.value(),
             elemental.GetAttr(DoubleAttr2::kAuxObjLinCoef, aux_lin_obj_var)});
      }
    }
    for (auto& [aux_obj, lin_terms] : lin_con_updates) {
      absl::c_sort(lin_terms);
      SparseDoubleVectorProto& proto_terms =
          *mods[aux_obj].mutable_linear_coefficients();
      for (auto [var, coef] : lin_terms) {
        proto_terms.add_ids(var);
        proto_terms.add_values(coef);
      }
    }
  }
  if (deleted.empty() && new_aux_objs.empty() &&
      result.objective_updates().empty()) {
    return std::nullopt;
  }
  *result.mutable_deleted_objective_ids() = std::move(deleted);
  for (const AuxiliaryObjectiveId id : new_aux_objs) {
    ASSIGN_OR_RETURN(((*result.mutable_new_objectives())[id.value()]),
                     ExportAuxiliaryObjective(elemental, id, remove_names));
  }
  return result;
}

absl::StatusOr<std::optional<ModelUpdateProto>> ExportModelUpdateProto(
    const Elemental& elemental, const Diff& diff, const bool remove_names) {
  const std::array<std::vector<int64_t>, kNumElements> new_elements =
      ElementsSinceCheckpointPerType(elemental, diff);
  if (ModelUpdateIsEmpty(diff, new_elements)) {
    return std::nullopt;
  }
  // Warning: further validation is required, see comments on
  // ValidateModelUpdateFitsInProto().
  RETURN_IF_ERROR(ValidateModelUpdateFitsInProto(diff, new_elements));

  ModelUpdateProto result;
  const int64_t var_checkpoint = diff.checkpoint(ElementType::kVariable);
  const ElementIdsSpan<ElementType::kVariable> new_var_ids(
      &new_elements[static_cast<int>(ElementType::kVariable)]);
  const ElementIdsSpan<ElementType::kLinearConstraint> new_lin_cons(
      &new_elements[static_cast<int>(ElementType::kLinearConstraint)]);
  const ElementIdsSpan<ElementType::kQuadraticConstraint> new_quad_cons(
      &new_elements[static_cast<int>(ElementType::kQuadraticConstraint)]);
  const ElementIdsSpan<ElementType::kIndicatorConstraint> new_ind_cons(
      &new_elements[static_cast<int>(ElementType::kIndicatorConstraint)]);
  const ElementIdsSpan<ElementType::kAuxiliaryObjective> new_aux_objs(
      &new_elements[static_cast<int>(ElementType::kAuxiliaryObjective)]);

  // Variables
  *result.mutable_deleted_variable_ids() =
      DeletedIdsSorted<ElementType::kVariable>(diff);
  if (auto var_updates = ExportVariableUpdates(elemental, diff);
      var_updates.has_value()) {
    *result.mutable_variable_updates() = *std::move(var_updates);
  }
  if (auto vars = ExportVariables(elemental, new_var_ids, remove_names);
      vars.has_value()) {
    *result.mutable_new_variables() = *std::move(vars);
  }

  // Objective
  {
    ASSIGN_OR_RETURN(std::optional<ObjectiveUpdatesProto> objective_updates,
                     ExportObjectiveUpdates(elemental, diff, new_var_ids));
    if (objective_updates.has_value()) {
      *result.mutable_objective_updates() = *std::move(objective_updates);
    }
  }
  // Auxiliary objectives
  {
    ASSIGN_OR_RETURN(
        std::optional<AuxiliaryObjectivesUpdatesProto> aux_objs,
        ExportAuxiliaryObjectivesUpdates(elemental, diff, new_var_ids,
                                         new_aux_objs, remove_names));
    if (aux_objs.has_value()) {
      *result.mutable_auxiliary_objectives_updates() = *std::move(aux_objs);
    }
  }

  // Linear constraints
  *result.mutable_deleted_linear_constraint_ids() =
      DeletedIdsSorted<ElementType::kLinearConstraint>(diff);
  if (auto lin_con_updates = ExportLinearConstraintUpdates(elemental, diff);
      lin_con_updates.has_value()) {
    *result.mutable_linear_constraint_updates() = *std::move(lin_con_updates);
  }
  if (auto lin_cons =
          ExportLinearConstraints(elemental, new_lin_cons, remove_names);
      lin_cons.has_value()) {
    *result.mutable_new_linear_constraints() = *std::move(lin_cons);
  }

  // Linear constraint matrix
  {
    std::vector<AttrKey<2>> mat_keys =
        elemental.ModifiedKeysThatExist(DoubleAttr2::kLinConCoef, diff);
    for (const VariableId new_var : new_var_ids) {
      for (AttrKey<2> related_key :
           elemental.Slice<1>(DoubleAttr2::kLinConCoef, new_var.value())) {
        mat_keys.push_back(related_key);
      }
    }
    for (const LinearConstraintId new_con : new_lin_cons) {
      for (AttrKey<2> related_key :
           elemental.Slice<0>(DoubleAttr2::kLinConCoef, new_con.value())) {
        // When related_var >= checkpoint, we got this case from the loop above.
        // We do at most twice as much work here as should from this.
        if (related_key[1] < var_checkpoint) {
          mat_keys.push_back(related_key);
        }
      }
    }
    RETURN_IF_ERROR(CanExportToProto(mat_keys.size()))
        << "too many linear constraint matrix nonzeros in model update";
    absl::c_sort(mat_keys);
    if (auto mat = ExportSparseDoubleMatrix(elemental, DoubleAttr2::kLinConCoef,
                                            mat_keys);
        mat.has_value()) {
      *result.mutable_linear_constraint_matrix_updates() = *std::move(mat);
    }
  }
  // Quadratic constraints
  {
    ASSIGN_OR_RETURN(
        std::optional<QuadraticConstraintUpdatesProto> quad_updates,
        ExportQuadraticConstraintsUpdates(elemental, diff, new_quad_cons,
                                          remove_names));
    if (quad_updates.has_value()) {
      *result.mutable_quadratic_constraint_updates() = *std::move(quad_updates);
    }
  }
  // Indicator constraints
  {
    ASSIGN_OR_RETURN(std::optional<IndicatorConstraintUpdatesProto> ind_updates,
                     ExportIndicatorConstraintsUpdates(
                         elemental, diff, new_ind_cons, remove_names));
    if (ind_updates.has_value()) {
      *result.mutable_indicator_constraint_updates() = *std::move(ind_updates);
    }
  }
  return result;
}

}  // namespace

absl::StatusOr<std::optional<ModelUpdateProto>> Elemental::ExportModelUpdate(
    const DiffHandle diff, const bool remove_names) const {
  if (&diff.diffs_ != diffs_.get()) {
    return util::InvalidArgumentErrorBuilder()
           << "diff with id: " << diff.id() << " is from another Elemental";
  }
  Diff* diff_value = diffs_->Get(diff.id());
  if (diff_value == nullptr) {
    return util::InvalidArgumentErrorBuilder()
           << "Model has no diff with id: " << diff.id();
  }
  // It intentional that that this function is implemented without access to the
  // private API of elemental. This allows us to change the implementation
  // elemental without breaking the proto export code.
  return ExportModelUpdateProto(*this, *diff_value, remove_names);
}

}  // namespace operations_research::math_opt
