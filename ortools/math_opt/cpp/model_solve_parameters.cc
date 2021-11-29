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
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/result.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

using ::google::protobuf::RepeatedField;

ModelSolveParameters ModelSolveParameters::OnlyPrimalVariables() {
  ModelSolveParameters parameters;
  parameters.dual_linear_constraints_filter =
      MakeSkipAllFilter<LinearConstraint>();
  parameters.dual_variables_filter = MakeSkipAllFilter<Variable>();
  return parameters;
}

ModelSolveParameters ModelSolveParameters::OnlySomePrimalVariables(
    std::initializer_list<Variable> variables) {
  return OnlySomePrimalVariables<std::initializer_list<Variable>>(variables);
}

IndexedModel* ModelSolveParameters::model() const {
  return internal::ConsistentModel({primal_variables_filter.model(),
                                    dual_linear_constraints_filter.model(),
                                    dual_variables_filter.model()});
}

ModelSolveParametersProto ModelSolveParameters::Proto() const {
  // We call model() here for its side effect of asserting that all filters use
  // variables and linear constraints use the same model.
  model();

  ModelSolveParametersProto ret;
  *ret.mutable_primal_variables_filter() = primal_variables_filter.Proto();
  *ret.mutable_dual_linear_constraints_filter() =
      dual_linear_constraints_filter.Proto();
  *ret.mutable_dual_variables_filter() = dual_variables_filter.Proto();

  // TODO(user): consolidate code. Probably best to add an export_to_proto
  // to IdMap
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
      constraint_status_values->Add(initial_basis->constraint_status.at(key));
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
      variable_status_values->Add(initial_basis->variable_status.at(key));
    }
  }
  return ret;
}

}  // namespace math_opt
}  // namespace operations_research
