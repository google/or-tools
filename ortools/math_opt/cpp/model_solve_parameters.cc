// Copyright 2010-2024 Google LLC
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

#include "ortools/math_opt/cpp/model_solve_parameters.h"

#include <stdint.h>

#include <initializer_list>
#include <optional>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/repeated_field.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/solution.h"
#include "ortools/math_opt/cpp/sparse_containers.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/util/status_macros.h"

namespace operations_research {
namespace math_opt {

using ::google::protobuf::RepeatedField;

ModelSolveParameters ModelSolveParameters::OnlyPrimalVariables() {
  ModelSolveParameters parameters;
  parameters.dual_values_filter = MakeSkipAllFilter<LinearConstraint>();
  parameters.quadratic_dual_values_filter =
      MakeSkipAllFilter<QuadraticConstraint>();
  parameters.reduced_costs_filter = MakeSkipAllFilter<Variable>();
  return parameters;
}

ModelSolveParameters ModelSolveParameters::OnlySomePrimalVariables(
    std::initializer_list<Variable> variables) {
  return OnlySomePrimalVariables<std::initializer_list<Variable>>(variables);
}

absl::Status ModelSolveParameters::CheckModelStorage(
    const ModelStorage* const expected_storage) const {
  for (const SolutionHint& hint : solution_hints) {
    RETURN_IF_ERROR(hint.CheckModelStorage(expected_storage))
        << "invalid hint in solution_hints";
  }
  if (initial_basis.has_value()) {
    RETURN_IF_ERROR(initial_basis->CheckModelStorage(expected_storage))
        << "invalid initial_basis";
  }
  RETURN_IF_ERROR(variable_values_filter.CheckModelStorage(expected_storage))
      << "invalid variable_values_filter";
  RETURN_IF_ERROR(dual_values_filter.CheckModelStorage(expected_storage))
      << "invalid dual_values_filter";
  RETURN_IF_ERROR(
      quadratic_dual_values_filter.CheckModelStorage(expected_storage))
      << "invalid quadratic_dual_values_filter";
  RETURN_IF_ERROR(reduced_costs_filter.CheckModelStorage(expected_storage))
      << "invalid reduced_costs_filter";
  for (const auto [var, unused] : branching_priorities) {
    RETURN_IF_ERROR(internal::CheckModelStorage(
        /*storage=*/var.storage(),
        /*expected_storage=*/expected_storage))
        << "invalid variable " << var << " in branching_priorities";
  }
  for (const auto& [objective, params] : objective_parameters) {
    RETURN_IF_ERROR(internal::CheckModelStorage(
        /*storage=*/objective.storage(),
        /*expected_storage=*/expected_storage))
        << "invalid objective " << objective << " in objective_parameters";
  }
  for (const LinearConstraint lazy_linear_constraint :
       lazy_linear_constraints) {
    RETURN_IF_ERROR(internal::CheckModelStorage(
        /*storage=*/lazy_linear_constraint.storage(),
        /*expected_storage=*/expected_storage))
        << "invalid LinearConstraint " << lazy_linear_constraint
        << " in lazy_linear_constraints";
  }
  return absl::OkStatus();
}

absl::Status ModelSolveParameters::SolutionHint::CheckModelStorage(
    const ModelStorage* expected_storage) const {
  for (const auto& [v, _] : variable_values) {
    RETURN_IF_ERROR(internal::CheckModelStorage(
        /*storage=*/v.storage(),
        /*expected_storage=*/expected_storage))
        << "invalid variable " << v << " in variable_values";
  }
  for (const auto& [c, _] : dual_values) {
    RETURN_IF_ERROR(internal::CheckModelStorage(
        /*storage=*/c.storage(),
        /*expected_storage=*/expected_storage))
        << "invalid constraint " << c << " in dual_values";
  }
  return absl::OkStatus();
}

SolutionHintProto ModelSolveParameters::SolutionHint::Proto() const {
  SolutionHintProto hint;
  *hint.mutable_variable_values() = VariableValuesToProto(variable_values);
  *hint.mutable_dual_values() = LinearConstraintValuesToProto(dual_values);
  return hint;
}

absl::StatusOr<ModelSolveParameters::SolutionHint>
ModelSolveParameters::SolutionHint::FromProto(
    const Model& model, const SolutionHintProto& hint_proto) {
  OR_ASSIGN_OR_RETURN3(
      VariableMap<double> variable_values,
      VariableValuesFromProto(model.storage(), hint_proto.variable_values()),
      _ << "failed to parse SolutionHintProto.variable_values");
  OR_ASSIGN_OR_RETURN3(LinearConstraintMap<double> dual_values,
                       LinearConstraintValuesFromProto(
                           model.storage(), hint_proto.dual_values()),
                       _ << "failed to parse SolutionHintProto.dual_values");
  return SolutionHint{
      .variable_values = std::move(variable_values),
      .dual_values = std::move(dual_values),
  };
}

absl::StatusOr<ObjectiveParametersProto>
ModelSolveParameters::ObjectiveParameters::Proto() const {
  ObjectiveParametersProto params;
  if (objective_degradation_absolute_tolerance) {
    params.set_objective_degradation_absolute_tolerance(
        *objective_degradation_absolute_tolerance);
  }
  if (objective_degradation_relative_tolerance) {
    params.set_objective_degradation_relative_tolerance(
        *objective_degradation_relative_tolerance);
  }
  if (time_limit < absl::InfiniteDuration()) {
    RETURN_IF_ERROR(util_time::EncodeGoogleApiProto(
        time_limit, params.mutable_time_limit()));
  }
  return params;
}

absl::StatusOr<ModelSolveParameters::ObjectiveParameters>
ModelSolveParameters::ObjectiveParameters::FromProto(
    const ObjectiveParametersProto& proto) {
  ObjectiveParameters result;
  if (proto.has_objective_degradation_absolute_tolerance()) {
    result.objective_degradation_absolute_tolerance =
        proto.objective_degradation_absolute_tolerance();
  }
  if (proto.has_objective_degradation_relative_tolerance()) {
    result.objective_degradation_relative_tolerance =
        proto.objective_degradation_relative_tolerance();
  }
  if (proto.has_time_limit()) {
    OR_ASSIGN_OR_RETURN3(result.time_limit,
                         util_time::DecodeGoogleApiProto(proto.time_limit()),
                         _ << "invalid time_limit");
  } else {
    result.time_limit = absl::InfiniteDuration();
  }
  return result;
}

// TODO: b/315974557 - Return an error if a RepeatedField is too long.
absl::StatusOr<ModelSolveParametersProto> ModelSolveParameters::Proto() const {
  ModelSolveParametersProto ret;
  *ret.mutable_variable_values_filter() = variable_values_filter.Proto();
  *ret.mutable_dual_values_filter() = dual_values_filter.Proto();
  *ret.mutable_quadratic_dual_values_filter() =
      quadratic_dual_values_filter.Proto();
  *ret.mutable_reduced_costs_filter() = reduced_costs_filter.Proto();

  if (initial_basis.has_value()) {
    *ret.mutable_initial_basis() = initial_basis->Proto();
  }

  for (const SolutionHint& solution_hint : solution_hints) {
    *ret.add_solution_hints() = solution_hint.Proto();
  }
  if (!branching_priorities.empty()) {
    RepeatedField<int64_t>& variable_ids =
        *ret.mutable_branching_priorities()->mutable_ids();
    RepeatedField<int32_t>& variable_values =
        *ret.mutable_branching_priorities()->mutable_values();
    variable_ids.Reserve(static_cast<int>(branching_priorities.size()));
    variable_values.Reserve(static_cast<int>(branching_priorities.size()));
    for (const Variable& key : SortedKeys(branching_priorities)) {
      variable_ids.Add(key.id());
      variable_values.Add(branching_priorities.at(key));
    }
  }
  for (const auto& [objective, params] : objective_parameters) {
    if (objective.id().has_value()) {
      OR_ASSIGN_OR_RETURN3(
          ((*ret.mutable_auxiliary_objective_parameters())[*objective.id()]),
          params.Proto(),
          _ << "invalid parameters for objective " << *objective.id());
    } else {
      OR_ASSIGN_OR_RETURN3(*ret.mutable_primary_objective_parameters(),
                           params.Proto(),
                           _ << "invalid parameters for primary objective");
    }
  }
  if (!lazy_linear_constraints.empty()) {
    RepeatedField<int64_t>& lazy_linear_constraint_ids =
        *ret.mutable_lazy_linear_constraint_ids();
    lazy_linear_constraint_ids.Reserve(
        static_cast<int>(lazy_linear_constraints.size()));
    for (const LinearConstraint lazy_linear_constraint :
         lazy_linear_constraints) {
      lazy_linear_constraint_ids.Add(lazy_linear_constraint.id());
    }
    absl::c_sort(lazy_linear_constraint_ids);
  }
  return ret;
}

absl::StatusOr<ModelSolveParameters> ModelSolveParameters::FromProto(
    const Model& model, const ModelSolveParametersProto& proto) {
  ModelSolveParameters result;
  OR_ASSIGN_OR_RETURN3(
      result.variable_values_filter,
      VariableFilterFromProto(model, proto.variable_values_filter()),
      _ << "invalid variable_values_filter");
  OR_ASSIGN_OR_RETURN3(
      result.dual_values_filter,
      LinearConstraintFilterFromProto(model, proto.dual_values_filter()),
      _ << "invalid dual_values_filter");
  OR_ASSIGN_OR_RETURN3(result.quadratic_dual_values_filter,
                       QuadraticConstraintFilterFromProto(
                           model, proto.quadratic_dual_values_filter()),
                       _ << "invalid quadratic_dual_values_filter");
  OR_ASSIGN_OR_RETURN3(
      result.reduced_costs_filter,
      VariableFilterFromProto(model, proto.reduced_costs_filter()),
      _ << "invalid reduced_costs_filter");
  if (proto.has_initial_basis()) {
    OR_ASSIGN_OR_RETURN3(
        result.initial_basis,
        Basis::FromProto(model.storage(), proto.initial_basis()),
        _ << "invalid initial_basis");
  }
  for (int i = 0; i < proto.solution_hints_size(); ++i) {
    OR_ASSIGN_OR_RETURN3(
        SolutionHint hint,
        SolutionHint::FromProto(model, proto.solution_hints(i)),
        _ << "invalid solution_hints[" << i << "]");
    result.solution_hints.push_back(std::move(hint));
  }
  OR_ASSIGN_OR_RETURN3(
      result.branching_priorities,
      VariableValuesFromProto(model.storage(), proto.branching_priorities()),
      _ << "invalid branching_priorities");
  if (proto.has_primary_objective_parameters()) {
    OR_ASSIGN_OR_RETURN3(
        auto primary_objective_params,
        ObjectiveParameters::FromProto(proto.primary_objective_parameters()),
        _ << "invalid primary_objective_parameters");
    result.objective_parameters.try_emplace(
        Objective::Primary(model.storage()),
        std::move(primary_objective_params));
  }
  for (const auto& [id, aux_obj_params_proto] :
       proto.auxiliary_objective_parameters()) {
    if (!model.has_auxiliary_objective(id)) {
      return util::InvalidArgumentErrorBuilder()
             << "invalid auxiliary_objective_parameters with id: " << id
             << ", objective not in the model";
    }
    OR_ASSIGN_OR_RETURN3(
        auto aux_obj_params,
        ObjectiveParameters::FromProto(aux_obj_params_proto),
        _ << "invalid auxiliary_objective_parameters with id: " << id);
    result.objective_parameters.try_emplace(
        Objective::Auxiliary(model.storage(), AuxiliaryObjectiveId{id}),
        std::move(aux_obj_params));
  }
  for (int64_t lin_con : proto.lazy_linear_constraint_ids()) {
    if (!model.has_linear_constraint(lin_con)) {
      return util::InvalidArgumentErrorBuilder()
             << "invalid lazy_linear_constraint with id: " << lin_con
             << ", constraint not in the model";
    }
    result.lazy_linear_constraints.insert(model.linear_constraint(lin_con));
  }
  return result;
}

}  // namespace math_opt
}  // namespace operations_research
