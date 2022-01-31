// Copyright 2010-2021 Google LLC
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
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/solution.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

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

const ModelStorage* ModelSolveParameters::storage() const {
  return internal::ConsistentModelStorage({variable_values_filter.storage(),
                                           dual_values_filter.storage(),
                                           reduced_costs_filter.storage()});
}

ModelSolveParametersProto ModelSolveParameters::Proto() const {
  // We call storage() here for its side effect of asserting that all filters
  // use variables and linear constraints use the same model.
  storage();

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
         initial_basis->constraint_status.SortedKeys()) {
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
    for (const Variable& key : initial_basis->variable_status.SortedKeys()) {
      variable_status_ids->Add(key.id());
      variable_status_values->Add(
          EnumToProto(initial_basis->variable_status.at(key)));
    }
  }
  for (const SolutionHint& solution_hint : solution_hints) {
    SolutionHintProto& hint = *ret.add_solution_hints();
    RepeatedField<int64_t>* const variable_ids =
        hint.mutable_variable_values()->mutable_ids();
    RepeatedField<double>* const variable_values =
        hint.mutable_variable_values()->mutable_values();
    variable_ids->Reserve(solution_hint.variable_values.size());
    variable_values->Reserve(solution_hint.variable_values.size());
    for (const Variable& key : solution_hint.variable_values.SortedKeys()) {
      variable_ids->Add(key.id());
      variable_values->Add(solution_hint.variable_values.at(key));
    }
  }
  if (!branching_priorities.empty()) {
    RepeatedField<int64_t>* const variable_ids =
        ret.mutable_branching_priorities()->mutable_ids();
    RepeatedField<int32_t>* const variable_values =
        ret.mutable_branching_priorities()->mutable_values();
    variable_ids->Reserve(branching_priorities.size());
    variable_values->Reserve(branching_priorities.size());
    for (const Variable& key : branching_priorities.SortedKeys()) {
      variable_ids->Add(key.id());
      variable_values->Add(branching_priorities.at(key));
    }
  }
  return ret;
}

}  // namespace math_opt
}  // namespace operations_research
