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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_MODEL_SOLVE_PARAMETERS_H_
#define OR_TOOLS_MATH_OPT_CPP_MODEL_SOLVE_PARAMETERS_H_

#include <sys/types.h>

#include <initializer_list>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/map_filter.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/solution.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

// Parameters to control a single solve that are specific to the input
// model (see SolveParametersProto for model independent parameters).
struct ModelSolveParameters {
  // Returns the parameters that empty DualSolution and DualRay, only keep the
  // values of all variables in PrimalSolution and PrimalRay.
  //
  // This is a shortcut method that is equivalent to setting the dual filters
  // with MakeSkipAllFilter().
  static ModelSolveParameters OnlyPrimalVariables();

  // Returns the parameters that empty DualSolution and DualRay, only keep the
  // values of the specified variables in PrimalSolution and PrimalRay.
  //
  // The input Collection must be usable in a for-range loop with Variable
  // values. This will be typically a std::vector<Variable> or and
  // std::initializer_list<Variable> (see the other overload).
  //
  // This is a shortcut method that is equivalent to setting the dual filters
  // with MakeSkipAllFilter() and the variable_values_filter with
  // MakeKeepKeysFilter(variables).
  //
  // Example:
  //   std::vector<Variable> decision_vars = ...;
  //   const auto params =
  //      ModelSolveParameters::OnlySomePrimalVariables(decision_vars);
  template <typename Collection>
  static ModelSolveParameters OnlySomePrimalVariables(
      const Collection& variables);

  // Returns the parameters that empty DualSolution and DualRay, only keeping
  // the values of the specified variables in PrimalSolution and PrimalRay.
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

  // The filter that is applied to variable_values of both PrimalSolution and
  // PrimalRay.
  MapFilter<Variable> variable_values_filter;

  // The filter that is applied to dual_values of DualSolution and DualRay.
  MapFilter<LinearConstraint> dual_values_filter;

  // The filter that is applied to reduced_costs of DualSolution and DualRay.
  MapFilter<Variable> reduced_costs_filter;

  // Optional initial basis for warm starting simplex LP solvers. If set, it is
  // expected to be valid.
  std::optional<Basis> initial_basis;

  // A solution hint. It can be partial and does not need to be feasible; see
  // solution_hints for details.
  struct SolutionHint {
    VariableMap<double> variable_values;
  };

  // Optional solution hints. If set, they are expected to consist of
  // assignments of finite values to primal or dual variables in the model (some
  // variables may lack assignments and the assignment does not necessarily have
  // to lead to a feasible solution).
  std::vector<SolutionHint> solution_hints;

  // Optional branching priorities. Variables with higher values will be
  // branched on first. Variables for which priorities are not set get the
  // solver's default priority (usualy zero). If set, they are expected to
  // consist of finite priorities for primal variables in the model.
  VariableMap<int32_t> branching_priorities;

  // Returns a failure if the referenced variables don't belong to the input
  // expected_storage (which must not be nullptr).
  absl::Status CheckModelStorage(const ModelStorage* expected_storage) const;

  // Returns the proto equivalent of this object.
  //
  // The caller should use CheckModelStorage() as this function does not check
  // internal consistency of the referenced variables and constraints.
  ModelSolveParametersProto Proto() const;
};

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename Collection>
ModelSolveParameters ModelSolveParameters::OnlySomePrimalVariables(
    const Collection& variables) {
  ModelSolveParameters parameters = OnlyPrimalVariables();
  parameters.variable_values_filter = MakeKeepKeysFilter(variables);
  return parameters;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_MODEL_SOLVE_PARAMETERS_H_
