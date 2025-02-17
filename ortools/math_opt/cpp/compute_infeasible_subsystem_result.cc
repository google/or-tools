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

#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"
#include "ortools/math_opt/constraints/sos/sos1_constraint.h"
#include "ortools/math_opt/constraints/sos/sos2_constraint.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/cpp/enums.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/validators/infeasible_subsystem_validator.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

ModelSubset::Bounds ModelSubset::Bounds::FromProto(
    const ModelSubsetProto::Bounds& bounds_proto) {
  return {.lower = bounds_proto.lower(), .upper = bounds_proto.upper()};
}

ModelSubsetProto::Bounds ModelSubset::Bounds::Proto() const {
  ModelSubsetProto::Bounds proto;
  proto.set_lower(lower);
  proto.set_upper(upper);
  return proto;
}

std::ostream& operator<<(std::ostream& out, const ModelSubset::Bounds& bounds) {
  const auto bool_to_str = [](const bool b) { return b ? "true" : "false"; };
  out << "{lower: " << bool_to_str(bounds.lower)
      << ", upper: " << bool_to_str(bounds.upper) << "}";
  return out;
}

namespace {

template <typename K>
absl::Status BoundsMapProtoToCpp(
    const google::protobuf::Map<int64_t, ModelSubsetProto::Bounds>& source,
    absl::flat_hash_map<K, ModelSubset::Bounds>& target,
    const ModelStorage* const model,
    bool (ModelStorage::* const contains_strong_id)(typename K::IdType id)
        const,
    const absl::string_view object_name) {
  for (const auto& [raw_id, bounds_proto] : source) {
    const typename K::IdType strong_id(raw_id);
    if (!(model->*contains_strong_id)(strong_id)) {
      return util::InvalidArgumentErrorBuilder()
             << "no " << object_name << " with id: " << raw_id;
    }
    target.insert(
        {K(model, strong_id), ModelSubset::Bounds::FromProto(bounds_proto)});
  }
  return absl::OkStatus();
}

template <typename K>
absl::Status RepeatedIdsProtoToCpp(
    const google::protobuf::RepeatedField<int64_t>& source,
    absl::flat_hash_set<K>& target, const ModelStorage* const model,
    bool (ModelStorage::* const contains_strong_id)(typename K::IdType id)
        const,
    const absl::string_view object_name) {
  for (const int64_t raw_id : source) {
    const typename K::IdType strong_id(raw_id);
    if (!(model->*contains_strong_id)(strong_id)) {
      return util::InvalidArgumentErrorBuilder()
             << "no " << object_name << " with id: " << raw_id;
    }
    target.insert(K(model, strong_id));
  }
  return absl::OkStatus();
}

template <typename K>
google::protobuf::Map<int64_t, ModelSubsetProto::Bounds> BoundsMapCppToProto(
    const absl::flat_hash_map<K, ModelSubset::Bounds> source) {
  google::protobuf::Map<int64_t, ModelSubsetProto::Bounds> result;
  for (const auto& [key, bounds] : source) {
    result.insert({key.id(), bounds.Proto()});
  }
  return result;
}

template <typename K>
google::protobuf::RepeatedField<int64_t> RepeatedIdsCppToProto(
    const absl::flat_hash_set<K>& source) {
  google::protobuf::RepeatedField<int64_t> result;
  for (const auto object : source) {
    result.Add(object.id());
  }
  absl::c_sort(result);
  return result;
}

}  // namespace

absl::StatusOr<ModelSubset> ModelSubset::FromProto(
    const ModelStorage* const model, const ModelSubsetProto& proto) {
  ModelSubset model_subset;
  RETURN_IF_ERROR(BoundsMapProtoToCpp(proto.variable_bounds(),
                                      model_subset.variable_bounds, model,
                                      &ModelStorage::has_variable, "variable"))
      << "element of variable_bounds";
  RETURN_IF_ERROR(RepeatedIdsProtoToCpp(
      proto.variable_integrality(), model_subset.variable_integrality, model,
      &ModelStorage::has_variable, "variable"))
      << "element of variable_integrality";
  RETURN_IF_ERROR(BoundsMapProtoToCpp(
      proto.linear_constraints(), model_subset.linear_constraints, model,
      &ModelStorage::has_linear_constraint, "linear constraint"));
  RETURN_IF_ERROR(BoundsMapProtoToCpp(
      proto.quadratic_constraints(), model_subset.quadratic_constraints, model,
      &ModelStorage::has_constraint, "quadratic constraint"));
  RETURN_IF_ERROR(RepeatedIdsProtoToCpp(
      proto.second_order_cone_constraints(),
      model_subset.second_order_cone_constraints, model,
      &ModelStorage::has_constraint, "second-order cone constraint"));
  RETURN_IF_ERROR(RepeatedIdsProtoToCpp(
      proto.sos1_constraints(), model_subset.sos1_constraints, model,
      &ModelStorage::has_constraint, "SOS1 constraint"));
  RETURN_IF_ERROR(RepeatedIdsProtoToCpp(
      proto.sos2_constraints(), model_subset.sos2_constraints, model,
      &ModelStorage::has_constraint, "SOS2 constraint"));
  RETURN_IF_ERROR(RepeatedIdsProtoToCpp(
      proto.indicator_constraints(), model_subset.indicator_constraints, model,
      &ModelStorage::has_constraint, "indicator constraint"));
  return model_subset;
}

ModelSubsetProto ModelSubset::Proto() const {
  ModelSubsetProto proto;
  *proto.mutable_variable_bounds() = BoundsMapCppToProto(variable_bounds);
  *proto.mutable_variable_integrality() =
      RepeatedIdsCppToProto(variable_integrality);
  *proto.mutable_linear_constraints() = BoundsMapCppToProto(linear_constraints);
  *proto.mutable_quadratic_constraints() =
      BoundsMapCppToProto(quadratic_constraints);
  *proto.mutable_second_order_cone_constraints() =
      RepeatedIdsCppToProto(second_order_cone_constraints);
  *proto.mutable_sos1_constraints() = RepeatedIdsCppToProto(sos1_constraints);
  *proto.mutable_sos2_constraints() = RepeatedIdsCppToProto(sos2_constraints);
  *proto.mutable_indicator_constraints() =
      RepeatedIdsCppToProto(indicator_constraints);
  return proto;
}

absl::Status ModelSubset::CheckModelStorage(
    const ModelStorage* const expected_storage) const {
  const auto validate_map_keys =
      [expected_storage](const auto& map,
                         const absl::string_view name) -> absl::Status {
    for (const auto& [key, unused] : map) {
      RETURN_IF_ERROR(
          internal::CheckModelStorage(key.storage(), expected_storage))
          << "invalid key " << key << " in " << name;
    }
    return absl::OkStatus();
  };
  const auto validate_set_elements =
      [expected_storage](const auto& set,
                         const absl::string_view name) -> absl::Status {
    for (const auto entry : set) {
      RETURN_IF_ERROR(
          internal::CheckModelStorage(entry.storage(), expected_storage))
          << "invalid entry " << entry << " in " << name;
    }
    return absl::OkStatus();
  };

  RETURN_IF_ERROR(validate_map_keys(variable_bounds, "variable_bounds"));
  RETURN_IF_ERROR(
      validate_set_elements(variable_integrality, "variable_integrality"));
  RETURN_IF_ERROR(validate_map_keys(linear_constraints, "linear_constraints"));
  RETURN_IF_ERROR(
      validate_map_keys(quadratic_constraints, "quadratic_constraints"));
  RETURN_IF_ERROR(validate_set_elements(second_order_cone_constraints,
                                        "second_order_cone_constraints"));
  RETURN_IF_ERROR(validate_set_elements(sos1_constraints, "sos1_constraints"));
  RETURN_IF_ERROR(validate_set_elements(sos2_constraints, "sos2_constraints"));
  RETURN_IF_ERROR(
      validate_set_elements(indicator_constraints, "indicator_constraints"));
  return absl::OkStatus();
}

bool ModelSubset::empty() const {
  return variable_bounds.empty() && variable_integrality.empty() &&
         linear_constraints.empty() && quadratic_constraints.empty() &&
         second_order_cone_constraints.empty() && sos1_constraints.empty() &&
         sos2_constraints.empty() && indicator_constraints.empty();
}

std::string ModelSubset::ToString() const {
  std::stringstream str;
  str << "Model Subset:\n";
  const auto stream_object = [&str](const auto& object) {
    str << "  " << object << ": " << object.ToString() << "\n";
  };
  const auto stream_bounded_object =
      [&str](const auto& object, const BoundedQuadraticExpression& as_expr,
             const Bounds& bounds) {
        if (bounds.empty()) {
          return;
        }
        // We only want to only print the bounds appearing in the subset. The <<
        // operator for `BoundedQuadraticExpression`s will ignore -/+inf bound
        // values for the lower/upper bounds, respectively (assuming that at
        // least one is finite, otherwise it chooses to print one bound
        // arbitrarily). So, to suppress bounds not in the subset, it suffices
        // to set their value to the appropriate infinity.
        const double lb = bounds.lower ? as_expr.lower_bound : -kInf;
        const double ub = bounds.upper ? as_expr.upper_bound : kInf;
        str << "  " << object << ": " << (lb <= as_expr.expression <= ub)
            << "\n";
      };

  str << " Variable bounds:\n";
  for (const Variable variable : SortedKeys(variable_bounds)) {
    stream_bounded_object(
        variable, variable.lower_bound() <= variable <= variable.upper_bound(),
        variable_bounds.at(variable));
  }
  str << " Variable integrality:\n";
  for (const Variable variable : SortedElements(variable_integrality)) {
    str << "  " << variable << "\n";
  }
  str << " Linear constraints:\n";
  for (const LinearConstraint constraint : SortedKeys(linear_constraints)) {
    stream_bounded_object(constraint, constraint.AsBoundedLinearExpression(),
                          linear_constraints.at(constraint));
  }
  if (!quadratic_constraints.empty()) {
    str << " Quadratic constraints:\n";
    for (const QuadraticConstraint constraint :
         SortedKeys(quadratic_constraints)) {
      stream_bounded_object(constraint,
                            constraint.AsBoundedQuadraticExpression(),
                            quadratic_constraints.at(constraint));
    }
  }
  if (!second_order_cone_constraints.empty()) {
    str << " Second-order cone constraints:\n";
    for (const SecondOrderConeConstraint constraint :
         SortedElements(second_order_cone_constraints)) {
      stream_object(constraint);
    }
  }
  if (!sos1_constraints.empty()) {
    str << " SOS1 constraints:\n";
    for (const Sos1Constraint constraint : SortedElements(sos1_constraints)) {
      stream_object(constraint);
    }
  }
  if (!sos2_constraints.empty()) {
    str << " SOS2 constraints:\n";
    for (const Sos2Constraint constraint : SortedElements(sos2_constraints)) {
      stream_object(constraint);
    }
  }
  if (!indicator_constraints.empty()) {
    str << " Indicator constraints:\n";
    for (const IndicatorConstraint constraint :
         SortedElements(indicator_constraints)) {
      stream_object(constraint);
    }
  }
  return str.str();
}

std::ostream& operator<<(std::ostream& out, const ModelSubset& model_subset) {
  const auto stream_bounds_map = [&out](const auto& map,
                                        const absl::string_view name) {
    out << name << ": {"
        << absl::StrJoin(SortedKeys(map), ", ",
                         [map](std::string* out, const auto& key) {
                           absl::StrAppendFormat(
                               out, "{%s, %s}", absl::FormatStreamed(key),
                               absl::FormatStreamed(map.at(key)));
                         })
        << "}";
  };
  const auto stream_set = [&out](const auto& set,
                                 const absl::string_view name) {
    out << name << ": {"
        << absl::StrJoin(SortedElements(set), ", ", absl::StreamFormatter())
        << "}";
  };

  out << "{";
  stream_bounds_map(model_subset.variable_bounds, "variable_bounds");
  out << ", ";
  stream_set(model_subset.variable_integrality, "variable_integrality");
  out << ", ";
  stream_bounds_map(model_subset.linear_constraints, "linear_constraints");
  out << ", ";
  stream_bounds_map(model_subset.quadratic_constraints,
                    "quadratic_constraints");
  out << ", ";
  stream_set(model_subset.second_order_cone_constraints,
             "second_order_cone_constraints");
  out << ", ";
  stream_set(model_subset.sos1_constraints, "sos1_constraints");
  out << ", ";
  stream_set(model_subset.sos2_constraints, "sos2_constraints");
  out << ", ";
  stream_set(model_subset.indicator_constraints, "indicator_constraints");
  out << "}";
  return out;
}

absl::StatusOr<ComputeInfeasibleSubsystemResult>
ComputeInfeasibleSubsystemResult::FromProto(
    const ModelStorage* const model,
    const ComputeInfeasibleSubsystemResultProto& result_proto) {
  ComputeInfeasibleSubsystemResult result;
  const std::optional<FeasibilityStatus> feasibility =
      EnumFromProto(result_proto.feasibility());
  if (!feasibility.has_value()) {
    return absl::InvalidArgumentError(
        "ComputeInfeasibleSubsystemResultProto.feasibility must be specified");
  }
  // We intentionally call this validator after checking `feasibility` so that
  // we can return a friendlier message for UNSPECIFIED.
  RETURN_IF_ERROR(
      ValidateComputeInfeasibleSubsystemResultNoModel(result_proto));
  result.feasibility = *feasibility;
  OR_ASSIGN_OR_RETURN3(
      result.infeasible_subsystem,
      ModelSubset::FromProto(model, result_proto.infeasible_subsystem()),
      _ << "invalid "
           "ComputeInfeasibleSubsystemResultProto.infeasible_subsystem");
  result.is_minimal = result_proto.is_minimal();
  return result;
}

ComputeInfeasibleSubsystemResultProto ComputeInfeasibleSubsystemResult::Proto()
    const {
  ComputeInfeasibleSubsystemResultProto proto;
  proto.set_feasibility(EnumToProto(feasibility));
  if (!infeasible_subsystem.empty()) {
    *proto.mutable_infeasible_subsystem() = infeasible_subsystem.Proto();
  }
  proto.set_is_minimal(is_minimal);
  return proto;
}

absl::Status ComputeInfeasibleSubsystemResult::CheckModelStorage(
    const ModelStorage* const expected_storage) const {
  return infeasible_subsystem.CheckModelStorage(expected_storage);
}

std::ostream& operator<<(std::ostream& out,
                         const ComputeInfeasibleSubsystemResult& result) {
  out << "{feasibility: " << result.feasibility
      << ", infeasible_subsystem: " << result.infeasible_subsystem
      << ", is_minimal: " << (result.is_minimal ? "true" : "false") << "}";
  return out;
}

}  // namespace operations_research::math_opt
