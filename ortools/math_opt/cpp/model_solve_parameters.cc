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

#include "ortools/math_opt/cpp/model_solve_parameters.h"

#include <stdint.h>

#include <initializer_list>
#include <optional>
#include <utility>

#include "google/protobuf/message.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
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
  RETURN_IF_ERROR(reduced_costs_filter.CheckModelStorage(expected_storage))
      << "invalid reduced_costs_filter";
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

ModelSolveParametersProto ModelSolveParameters::Proto() const {
  ModelSolveParametersProto ret;
  *ret.mutable_variable_values_filter() = variable_values_filter.Proto();
  *ret.mutable_dual_values_filter() = dual_values_filter.Proto();
  *ret.mutable_reduced_costs_filter() = reduced_costs_filter.Proto();

  // TODO(b/183616124): consolidate code. Probably best to add an
  // export_to_proto to IdMap
  if (initial_basis) {
    RepeatedField<int64_t>* const constraint_status_ids =
        ret.mutable_initial_basis()->mutable_constraint_status()->mutable_ids();
    RepeatedField<int>* const constraint_status_values =
        ret.mutable_initial_basis()
            ->mutable_constraint_status()
            ->mutable_values();
    constraint_status_ids->Reserve(initial_basis->constraint_status.size());
    constraint_status_values->Reserve(initial_basis->constraint_status.size());
    for (const LinearConstraint& key :
         SortedKeys(initial_basis->constraint_status)) {
      constraint_status_ids->Add(key.id());
      constraint_status_values->Add(
          EnumToProto(initial_basis->constraint_status.at(key)));
    }
    RepeatedField<int64_t>* const variable_status_ids =
        ret.mutable_initial_basis()->mutable_variable_status()->mutable_ids();
    RepeatedField<int>* const variable_status_values =
        ret.mutable_initial_basis()
            ->mutable_variable_status()
            ->mutable_values();
    variable_status_ids->Reserve(initial_basis->variable_status.size());
    variable_status_values->Reserve(initial_basis->variable_status.size());
    for (const Variable& key : SortedKeys(initial_basis->variable_status)) {
      variable_status_ids->Add(key.id());
      variable_status_values->Add(
          EnumToProto(initial_basis->variable_status.at(key)));
    }
  }
  for (const SolutionHint& solution_hint : solution_hints) {
    *ret.add_solution_hints() = solution_hint.Proto();
  }
  if (!branching_priorities.empty()) {
    RepeatedField<int64_t>* const variable_ids =
        ret.mutable_branching_priorities()->mutable_ids();
    RepeatedField<int32_t>* const variable_values =
        ret.mutable_branching_priorities()->mutable_values();
    variable_ids->Reserve(branching_priorities.size());
    variable_values->Reserve(branching_priorities.size());
    for (const Variable& key : SortedKeys(branching_priorities)) {
      variable_ids->Add(key.id());
      variable_values->Add(branching_priorities.at(key));
    }
  }
  return ret;
}

}  // namespace math_opt
}  // namespace operations_research
