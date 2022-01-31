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

#include "ortools/math_opt/cpp/solution.h"

#include <optional>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

template <typename Key>
IdMap<Key, double> ValuesFrom(const ModelStorage* const model,
                              const SparseDoubleVectorProto& vars_proto) {
  return IdMap<Key, double>(
      model, MakeView(vars_proto).as_map<typename Key::IdType>());
}

template <typename Key>
IdMap<Key, BasisStatus> BasisValues(
    const ModelStorage* const model,
    const SparseBasisStatusVector& basis_proto) {
  absl::flat_hash_map<typename Key::IdType, BasisStatus> id_map;
  for (const auto& [id, basis_status_proto] : MakeView(basis_proto)) {
    // CHECK fails on BASIS_STATUS_UNSPECIFIED (the validation code should have
    // tested that).
    // We need to cast because the C++ proto API stores repeated enums as ints.
    //
    // On top of that iOS 11 does not support .value() on optionals so we must
    // use operator*.
    const std::optional<BasisStatus> basis_status =
        EnumFromProto(static_cast<BasisStatusProto>(basis_status_proto));
    CHECK(basis_status.has_value());
    id_map[static_cast<typename Key::IdType>(id)] = *basis_status;
  }
  return IdMap<Key, BasisStatus>(model, std::move(id_map));
}

}  // namespace

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

std::optional<absl::string_view> Enum<BasisStatus>::ToOptString(
    BasisStatus value) {
  switch (value) {
    case BasisStatus::kFree:
      return "free";
    case BasisStatus::kAtLowerBound:
      return "at_lower_bound";
    case BasisStatus::kAtUpperBound:
      return "at_upper_bound";
    case BasisStatus::kFixedValue:
      return "fixed_value";
    case BasisStatus::kBasic:
      return "basic";
  }
  return std::nullopt;
}

absl::Span<const BasisStatus> Enum<BasisStatus>::AllValues() {
  static constexpr BasisStatus kBasisStatusValues[] = {
      BasisStatus::kFree,         BasisStatus::kAtLowerBound,
      BasisStatus::kAtUpperBound, BasisStatus::kFixedValue,
      BasisStatus::kBasic,
  };
  return absl::MakeConstSpan(kBasisStatusValues);
}

PrimalSolution PrimalSolution::FromProto(
    const ModelStorage* model,
    const PrimalSolutionProto& primal_solution_proto) {
  PrimalSolution primal_solution;
  primal_solution.variable_values =
      ValuesFrom<Variable>(model, primal_solution_proto.variable_values());
  primal_solution.objective_value = primal_solution_proto.objective_value();
  // TODO(b/209014770): consider adding a function to simplify this pattern.
  const std::optional<SolutionStatus> feasibility_status =
      EnumFromProto(primal_solution_proto.feasibility_status());
  CHECK(feasibility_status.has_value());
  primal_solution.feasibility_status = *feasibility_status;
  return primal_solution;
}

PrimalRay PrimalRay::FromProto(const ModelStorage* model,
                               const PrimalRayProto& primal_ray_proto) {
  return {.variable_values =
              ValuesFrom<Variable>(model, primal_ray_proto.variable_values())};
}

DualSolution DualSolution::FromProto(
    const ModelStorage* model, const DualSolutionProto& dual_solution_proto) {
  DualSolution dual_solution;
  dual_solution.dual_values =
      ValuesFrom<LinearConstraint>(model, dual_solution_proto.dual_values());
  dual_solution.reduced_costs =
      ValuesFrom<Variable>(model, dual_solution_proto.reduced_costs());
  if (dual_solution_proto.has_objective_value()) {
    dual_solution.objective_value = dual_solution_proto.objective_value();
  }
  // TODO(b/209014770): consider adding a function to simplify this pattern.
  const std::optional<SolutionStatus> feasibility_status =
      EnumFromProto(dual_solution_proto.feasibility_status());
  CHECK(feasibility_status.has_value());
  dual_solution.feasibility_status = *feasibility_status;
  return dual_solution;
}

DualRay DualRay::FromProto(const ModelStorage* model,
                           const DualRayProto& dual_ray_proto) {
  return {.dual_values =
              ValuesFrom<LinearConstraint>(model, dual_ray_proto.dual_values()),
          .reduced_costs =
              ValuesFrom<Variable>(model, dual_ray_proto.reduced_costs())};
}

Basis Basis::FromProto(const ModelStorage* model,
                       const BasisProto& basis_proto) {
  Basis basis;
  basis.constraint_status =
      BasisValues<LinearConstraint>(model, basis_proto.constraint_status());
  basis.variable_status =
      BasisValues<Variable>(model, basis_proto.variable_status());
  // TODO(b/209014770): consider adding a function to simplify this pattern.
  const std::optional<SolutionStatus> basic_dual_feasibility =
      EnumFromProto(basis_proto.basic_dual_feasibility());
  CHECK(basic_dual_feasibility.has_value());
  basis.basic_dual_feasibility = *basic_dual_feasibility;
  return basis;
}

Solution Solution::FromProto(const ModelStorage* model,
                             const SolutionProto& solution_proto) {
  Solution solution;
  if (solution_proto.has_primal_solution()) {
    solution.primal_solution =
        PrimalSolution::FromProto(model, solution_proto.primal_solution());
  }
  if (solution_proto.has_dual_solution()) {
    solution.dual_solution =
        DualSolution::FromProto(model, solution_proto.dual_solution());
  }
  if (solution_proto.has_basis()) {
    solution.basis = Basis::FromProto(model, solution_proto.basis());
  }
  return solution;
}

}  // namespace math_opt
}  // namespace operations_research
