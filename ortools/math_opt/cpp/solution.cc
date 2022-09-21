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

#include "ortools/math_opt/cpp/solution.h"

#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/sparse_containers.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"
#include "ortools/util/status_macros.h"

namespace operations_research {
namespace math_opt {

std::optional<absl::string_view> Enum<SolutionStatus>::ToOptString(
    SolutionStatus value) {
  switch (value) {
    case SolutionStatus::kFeasible:
      return "feasible";
    case SolutionStatus::kInfeasible:
      return "infeasible";
    case SolutionStatus::kUndetermined:
      return "undetermined";
  }
  return std::nullopt;
}

absl::Span<const SolutionStatus> Enum<SolutionStatus>::AllValues() {
  static constexpr SolutionStatus kSolutionStatusValues[] = {
      SolutionStatus::kFeasible,
      SolutionStatus::kInfeasible,
      SolutionStatus::kUndetermined,
  };
  return absl::MakeConstSpan(kSolutionStatusValues);
}

absl::StatusOr<PrimalSolution> PrimalSolution::FromProto(
    const ModelStorage* model,
    const PrimalSolutionProto& primal_solution_proto) {
  PrimalSolution primal_solution;
  OR_ASSIGN_OR_RETURN3(
      primal_solution.variable_values,
      VariableValuesFromProto(model, primal_solution_proto.variable_values()),
      _ << "invalid variable_values");
  primal_solution.objective_value = primal_solution_proto.objective_value();
  const std::optional<SolutionStatus> feasibility_status =
      EnumFromProto(primal_solution_proto.feasibility_status());
  if (!feasibility_status.has_value()) {
    return absl::InvalidArgumentError("feasibility_status must be specified");
  }
  primal_solution.feasibility_status = *feasibility_status;
  return primal_solution;
}

PrimalSolutionProto PrimalSolution::Proto() const {
  PrimalSolutionProto result;
  *result.mutable_variable_values() = VariableValuesToProto(variable_values);
  result.set_objective_value(objective_value);
  result.set_feasibility_status(EnumToProto(feasibility_status));
  return result;
}

absl::StatusOr<PrimalRay> PrimalRay::FromProto(
    const ModelStorage* model, const PrimalRayProto& primal_ray_proto) {
  PrimalRay result;
  OR_ASSIGN_OR_RETURN3(
      result.variable_values,
      VariableValuesFromProto(model, primal_ray_proto.variable_values()),
      _ << "invalid variable_values");
  return result;
}

PrimalRayProto PrimalRay::Proto() const {
  PrimalRayProto result;
  *result.mutable_variable_values() = VariableValuesToProto(variable_values);
  return result;
}

absl::StatusOr<DualSolution> DualSolution::FromProto(
    const ModelStorage* model, const DualSolutionProto& dual_solution_proto) {
  DualSolution dual_solution;
  OR_ASSIGN_OR_RETURN3(
      dual_solution.dual_values,
      LinearConstraintValuesFromProto(model, dual_solution_proto.dual_values()),
      _ << "invalid dual_values");
  OR_ASSIGN_OR_RETURN3(
      dual_solution.reduced_costs,
      VariableValuesFromProto(model, dual_solution_proto.reduced_costs()),
      _ << "invalid reduced_costs");
  if (dual_solution_proto.has_objective_value()) {
    dual_solution.objective_value = dual_solution_proto.objective_value();
  }
  const std::optional<SolutionStatus> feasibility_status =
      EnumFromProto(dual_solution_proto.feasibility_status());
  if (!feasibility_status.has_value()) {
    return absl::InvalidArgumentError("feasibility_status must be specified");
  }
  dual_solution.feasibility_status = *feasibility_status;
  return dual_solution;
}

DualSolutionProto DualSolution::Proto() const {
  DualSolutionProto result;
  *result.mutable_dual_values() = LinearConstraintValuesToProto(dual_values);
  *result.mutable_reduced_costs() = VariableValuesToProto(reduced_costs);
  if (objective_value.has_value()) {
    result.set_objective_value(*objective_value);
  }
  result.set_feasibility_status(EnumToProto(feasibility_status));
  return result;
}

absl::StatusOr<DualRay> DualRay::FromProto(const ModelStorage* model,
                                           const DualRayProto& dual_ray_proto) {
  DualRay result;
  OR_ASSIGN_OR_RETURN3(
      result.dual_values,
      LinearConstraintValuesFromProto(model, dual_ray_proto.dual_values()),
      _ << "invalid dual_values");
  OR_ASSIGN_OR_RETURN3(
      result.reduced_costs,
      VariableValuesFromProto(model, dual_ray_proto.reduced_costs()),
      _ << "invalid reduced_costs");
  return result;
}

DualRayProto DualRay::Proto() const {
  DualRayProto result;
  *result.mutable_dual_values() = LinearConstraintValuesToProto(dual_values);
  *result.mutable_reduced_costs() = VariableValuesToProto(reduced_costs);
  return result;
}

absl::StatusOr<Basis> Basis::FromProto(const ModelStorage* model,
                                       const BasisProto& basis_proto) {
  Basis basis;
  OR_ASSIGN_OR_RETURN3(
      basis.constraint_status,
      LinearConstraintBasisFromProto(model, basis_proto.constraint_status()),
      _ << "invalid constraint_status");
  OR_ASSIGN_OR_RETURN3(
      basis.variable_status,
      VariableBasisFromProto(model, basis_proto.variable_status()),
      _ << "invalid variable_status");
  const std::optional<SolutionStatus> basic_dual_feasibility =
      EnumFromProto(basis_proto.basic_dual_feasibility());
  if (!basic_dual_feasibility.has_value()) {
    return absl::InvalidArgumentError(
        "basic_dual_feasibility for a basis must be specified");
  }
  basis.basic_dual_feasibility = *basic_dual_feasibility;
  return basis;
}

BasisProto Basis::Proto() const {
  BasisProto result;
  *result.mutable_constraint_status() =
      LinearConstraintBasisToProto(constraint_status);
  *result.mutable_variable_status() = VariableBasisToProto(variable_status);
  result.set_basic_dual_feasibility(EnumToProto(basic_dual_feasibility));
  return result;
}

absl::StatusOr<Solution> Solution::FromProto(
    const ModelStorage* model, const SolutionProto& solution_proto) {
  Solution solution;
  if (solution_proto.has_primal_solution()) {
    OR_ASSIGN_OR_RETURN3(
        solution.primal_solution,
        PrimalSolution::FromProto(model, solution_proto.primal_solution()),
        _ << "invalid primal_solution");
  }
  if (solution_proto.has_dual_solution()) {
    OR_ASSIGN_OR_RETURN3(
        solution.dual_solution,
        DualSolution::FromProto(model, solution_proto.dual_solution()),
        _ << "invalid dual_solution");
  }
  if (solution_proto.has_basis()) {
    OR_ASSIGN_OR_RETURN3(solution.basis,
                         Basis::FromProto(model, solution_proto.basis()),
                         _ << "invalid basis");
  }
  return solution;
}

SolutionProto Solution::Proto() const {
  SolutionProto result;
  if (primal_solution.has_value()) {
    *result.mutable_primal_solution() = primal_solution->Proto();
  }
  if (dual_solution.has_value()) {
    *result.mutable_dual_solution() = dual_solution->Proto();
  }
  if (basis.has_value()) {
    *result.mutable_basis() = basis->Proto();
  }
  return result;
}

}  // namespace math_opt
}  // namespace operations_research
