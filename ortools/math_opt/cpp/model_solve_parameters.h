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

#ifndef OR_TOOLS_MATH_OPT_CPP_MODEL_SOLVE_PARAMETERS_H_
#define OR_TOOLS_MATH_OPT_CPP_MODEL_SOLVE_PARAMETERS_H_

#include <initializer_list>
#include <optional>

#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/map_filter.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/result.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/model_parameters.pb.h"

namespace operations_research {
namespace math_opt {

// Parameters to control a single solve that that are specific to the input
// model (see SolveParametersProto for model independent parameters).
struct ModelSolveParameters {
  // Returns the parameters that empty Result::DualSolution and Result::DualRay,
  // only keep the values of all variables in Result::PrimalSolution and
  // Result::PrimalRay.
  //
  // This is a shortcut method that is equivalent to setting the dual filters
  // with MakeSkipAllFilter().
  static ModelSolveParameters OnlyPrimalVariables();

  // Returns the parameters that empty Result::DualSolution and Result::DualRay,
  // only keep the values of the specified variables in Result::PrimalSolution
  // and Result::PrimalRay.
  //
  // The input Collection must be usable in a for-range loop with Variable
  // values. This will be typically a std::vector<Variable> or and
  // std::initializer_list<Variable> (see the other overload).
  //
  // This is a shortcut method that is equivalent to setting the dual filters
  // with MakeSkipAllFilter() and the primal_variables_filter with
  // MakeKeepKeysFilter(variables).
  //
  // Example:
  //   std::vector<Variable> decision_vars = ...;
  //   const auto params =
  //      ModelSolveParameters::OnlySomePrimalVariables(decision_vars);
  template <typename Collection>
  static ModelSolveParameters OnlySomePrimalVariables(
      const Collection& variables);

  // Returns the parameters that empty Result::DualSolution and Result::DualRay,
  // only keeping the values of the specified variables in
  // Result::PrimalSolution and Result::PrimalRay.
  //
  // See the other overload's documentation for details. This overload is needed
  // since C++ can't guess the type when using an initializer list expression.
  //
  // Example:
  //   const Variable a = ...;
  //   const Variable b = ...;
  //   const auto params =
  //      ModelSolveParameters::OnlySomePrimalVariables({a, b});
  static ModelSolveParameters OnlySomePrimalVariables(
      std::initializer_list<Variable> variables);

  // The filter that is applied to variable_values of both
  // Result::PrimalSolution and Result::PrimalRay.
  MapFilter<Variable> primal_variables_filter;

  // The filter that is applied to dual_values of Result::DualSolution and
  // Result::DualRay.
  MapFilter<LinearConstraint> dual_linear_constraints_filter;

  // The filter that is applied to reduced_costs of Result::DualSolution and
  // Result::DualRay.
  MapFilter<Variable> dual_variables_filter;

  // Optional initial basis for warm starting simplex LP solvers. If set, it is
  // expected to be valid.
  absl::optional<Result::Basis> initial_basis;

  // Returns the model of filtered keys. It returns a non-null value if and only
  // if one of the filters have a set and non empty filtered_keys().
  //
  // Asserts (using CHECK) that all variables and linear constraints referenced
  // by the filters are in the same model.
  IndexedModel* model() const;

  // Returns a new proto corresponding to these parameters.
  //
  // Asserts (using CHECK) that all variables and linear constraints referenced
  // by the filters are in the same model.
  ModelSolveParametersProto Proto() const;
};

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename Collection>
ModelSolveParameters ModelSolveParameters::OnlySomePrimalVariables(
    const Collection& variables) {
  ModelSolveParameters parameters = OnlyPrimalVariables();
  parameters.primal_variables_filter = MakeKeepKeysFilter(variables);
  return parameters;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_MODEL_SOLVE_PARAMETERS_H_
