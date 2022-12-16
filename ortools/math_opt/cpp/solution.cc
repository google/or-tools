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

#include "ortools/math_opt/cpp/solution.h"

#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/sparse_containers.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
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
    const ModelStorageCPtr model,
    const PrimalSolutionProto& primal_solution_proto) {
  PrimalSolution primal_solution;
  OR_ASSIGN_OR_RETURN3(
      primal_solution.variable_values,
      VariableValuesFromProto(model, primal_solution_proto.variable_values()),
      _ << "invalid variable_values");
  primal_solution.objective_value = primal_solution_proto.objective_value();
  OR_ASSIGN_OR_RETURN3(
      primal_solution.auxiliary_objective_values,
      AuxiliaryObjectiveValuesFromProto(
          model, primal_solution_proto.auxiliary_objective_values()),
      _ << "invalid auxiliary_objective_values");
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
  *result.mutable_auxiliary_objective_values() =
      AuxiliaryObjectiveValuesToProto(auxiliary_objective_values);
  result.set_feasibility_status(EnumToProto(feasibility_status));
  return result;
}

double PrimalSolution::get_objective_value(const Objective objective) const {
  if (!variable_values.empty()) {
    // Here we assume all keys are in the same storage. As PrimalSolution is not
    // properly encapsulated, we can't maintain a ModelStorage pointer and
    // iterating on all keys would have a too high cost.
    CHECK_EQ(variable_values.begin()->first.storage(), objective.storage());
  }
  if (!objective.is_primary()) {
    CHECK(auxiliary_objective_values.contains(objective));
    return auxiliary_objective_values.at(objective);
  }
  return objective_value;
}

absl::StatusOr<PrimalRay> PrimalRay::FromProto(
    const ModelStorageCPtr model, const PrimalRayProto& primal_ray_proto) {
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
    const ModelStorageCPtr model,
    const DualSolutionProto& dual_solution_proto) {
  DualSolution dual_solution;
  OR_ASSIGN_OR_RETURN3(
      dual_solution.dual_values,
      LinearConstraintValuesFromProto(model, dual_solution_proto.dual_values()),
      _ << "invalid dual_values");
  OR_ASSIGN_OR_RETURN3(dual_solution.quadratic_dual_values,
                       QuadraticConstraintValuesFromProto(
                           model, dual_solution_proto.quadratic_dual_values()),
                       _ << "invalid quadratic_dual_values");
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
  *result.mutable_quadratic_dual_values() =
      QuadraticConstraintValuesToProto(quadratic_dual_values);
  *result.mutable_reduced_costs() = VariableValuesToProto(reduced_costs);
  if (objective_value.has_value()) {
    result.set_objective_value(*objective_value);
  }
  result.set_feasibility_status(EnumToProto(feasibility_status));
  return result;
}

absl::StatusOr<DualRay> DualRay::FromProto(const ModelStorageCPtr model,
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

absl::StatusOr<Basis> Basis::FromProto(const ModelStorageCPtr model,
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
  basis.basic_dual_feasibility =
      EnumFromProto(basis_proto.basic_dual_feasibility());
  return basis;
}

absl::Status Basis::CheckModelStorage(
    const ModelStorageCPtr expected_storage) const {
  for (const auto& [v, _] : variable_status) {
    RETURN_IF_ERROR(
        internal::CheckModelStorage(/*storage=*/v.storage(),
                                    /*expected_storage=*/expected_storage))
        << "invalid variable " << v << " in variable_status";
  }
  for (const auto& [c, _] : constraint_status) {
    RETURN_IF_ERROR(
        internal::CheckModelStorage(/*storage=*/c.storage(),
                                    /*expected_storage=*/expected_storage))
        << "invalid constraint " << c << " in constraint_status";
  }
  return absl::OkStatus();
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
    const ModelStorageCPtr model, const SolutionProto& solution_proto) {
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
