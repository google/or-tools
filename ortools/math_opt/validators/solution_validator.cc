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

#include "ortools/math_opt/validators/solution_validator.h"

#include <cstdint>
#include <limits>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research {
namespace math_opt {
namespace {

absl::Status ValidateSolutionStatus(const SolutionStatusProto& status) {
  if (!SolutionStatusProto_IsValid(status)) {
    return absl::InvalidArgumentError(absl::StrCat("status = ", status));
  }
  if (status == SOLUTION_STATUS_UNSPECIFIED) {
    return absl::InvalidArgumentError("status = SOLUTION_STATUS_UNSPECIFIED");
  }
  return absl::OkStatus();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Solutions & Rays
////////////////////////////////////////////////////////////////////////////////

namespace {

// Validates that all pairs in the input view match the provided filter and that
// all expected values are there when skip_zero_values is not used.
//
// This function assumes caller have checked already that the input
// vector_view.ids() and the input filter are valid.
template <typename T>
absl::Status IsFiltered(const SparseVectorView<T>& vector_view,
                        const SparseVectorFilterProto& filter,
                        const IdNameBiMap& all_items) {
  RETURN_IF_ERROR(CheckIdsAndValuesSize(vector_view));
  SparseVectorFilterPredicate predicate(filter);
  RETURN_IF_ERROR(CheckIdsSubset(vector_view.ids(), all_items, "sparse vector",
                                 "model IDs"));
  for (int i = 0; i < vector_view.ids_size(); ++i) {
    const int64_t id = vector_view.ids(i);
    if (!predicate.AcceptsAndUpdate(id, vector_view.values(i))) {
      return absl::InvalidArgumentError(
          absl::StrCat("sparse vector should not contain the pair (id: ", id,
                       ", value: ", vector_view.values(i), ") (at index: ", i,
                       ") that should have been filtered"));
    }
  }

  // We don't test the length on the input if we skipped the zeros since missing
  // values are expected.
  if (filter.skip_zero_values()) {
    return absl::OkStatus();
  }

  const int expected_size =
      filter.filter_by_ids() ? filter.filtered_ids_size() : all_items.Size();
  if (vector_view.ids_size() != expected_size) {
    return absl::InvalidArgumentError(absl::StrCat(
        "sparse vector should contain ", expected_size, " values but contains ",
        vector_view.ids_size(), " instead"));
  }

  return absl::OkStatus();
}

// A solution vector is valid if:
//  * it is a valid SparseDoubleVectorProto.
//  * its values are finite.
//  * it contains only elements that pass the filter
//  * it contains all elements that pass the filter when skip_zero_values is not
//    used.
//
// TODO(b/174345677): check that the ids are valid.
absl::Status IsValidSolutionVector(const SparseDoubleVectorProto& vector,
                                   const SparseVectorFilterProto& filter,
                                   const IdNameBiMap& all_items) {
  const auto vector_view = MakeView(vector);
  RETURN_IF_ERROR(CheckIdsAndValues(
      vector_view,
      {.allow_positive_infinity = false, .allow_negative_infinity = false}));
  RETURN_IF_ERROR(IsFiltered(vector_view, filter, all_items));
  return absl::OkStatus();
}

}  // namespace

absl::Status ValidateSolution(const SolutionProto& solution,
                              const ModelSolveParametersProto& parameters,
                              const ModelSummary& model_summary) {
  if (!solution.has_primal_solution() && !solution.has_dual_solution() &&
      !solution.has_basis()) {
    return absl::InvalidArgumentError("empty solution");
  }
  if (solution.has_primal_solution()) {
    RETURN_IF_ERROR(ValidatePrimalSolution(solution.primal_solution(),
                                           parameters.variable_values_filter(),
                                           model_summary))
        << "invalid primal_solution";
  }
  if (solution.has_dual_solution()) {
    RETURN_IF_ERROR(ValidateDualSolution(solution.dual_solution(), parameters,
                                         model_summary))
        << "invalid dual_solution";
  }
  if (solution.has_basis()) {
    RETURN_IF_ERROR(ValidateBasis(solution.basis(), model_summary))
        << "invalid basis";
  }
  // TODO(b/204457524): consider checking equality of statuses for single-sided
  // LPs.
  if (solution.has_basis() && solution.has_dual_solution()) {
    if (solution.basis().basic_dual_feasibility() == SOLUTION_STATUS_FEASIBLE &&
        solution.dual_solution().feasibility_status() !=
            SOLUTION_STATUS_FEASIBLE) {
      return absl::InvalidArgumentError(
          "incompatible basis and dual solution: basis is dual feasible, but "
          "dual solution is not feasible");
    }
    if (solution.dual_solution().feasibility_status() ==
            SOLUTION_STATUS_INFEASIBLE &&
        solution.basis().basic_dual_feasibility() !=
            SOLUTION_STATUS_INFEASIBLE) {
      return absl::InvalidArgumentError(
          "incompatible basis and dual solution: dual solution is infeasible, "
          "but basis is not dual infeasible");
    }
  }
  return absl::OkStatus();
}

absl::Status ValidatePrimalSolutionVector(const SparseDoubleVectorProto& vector,
                                          const SparseVectorFilterProto& filter,
                                          const ModelSummary& model_summary) {
  RETURN_IF_ERROR(
      IsValidSolutionVector(vector, filter, model_summary.variables));
  return absl::OkStatus();
}

absl::Status ValidatePrimalSolution(const PrimalSolutionProto& primal_solution,
                                    const SparseVectorFilterProto& filter,
                                    const ModelSummary& model_summary) {
  RETURN_IF_ERROR(ValidateSolutionStatus(primal_solution.feasibility_status()))
      << "invalid PrimalSolutionProto.feasibility_status";
  RETURN_IF_ERROR(ValidatePrimalSolutionVector(
      primal_solution.variable_values(), filter, model_summary))
      << "invalid PrimalSolutionProto.variable_values";
  RETURN_IF_ERROR(CheckScalarNoNanNoInf(primal_solution.objective_value()))
      << "invalid PrimalSolutionProto.objective_value";
  for (const auto [id, obj_value] :
       primal_solution.auxiliary_objective_values()) {
    if (!model_summary.auxiliary_objectives.HasId(id)) {
      return util::InvalidArgumentErrorBuilder()
             << "unrecognized auxiliary objective ID: " << id
             << "; invalid PrimalSolutionProto.auxiliary_objective_values";
    }
    RETURN_IF_ERROR(CheckScalarNoNanNoInf(obj_value))
        << "invalid PrimalSolutionProto.auxiliary_objective_values";
  }
  return absl::OkStatus();
}

absl::Status ValidatePrimalRay(const PrimalRayProto& primal_ray,
                               const SparseVectorFilterProto& filter,
                               const ModelSummary& model_summary) {
  RETURN_IF_ERROR(IsValidSolutionVector(primal_ray.variable_values(), filter,
                                        model_summary.variables))
      << "invalid PrimalRayProto.variable_values";
  return absl::OkStatus();
}

absl::Status ValidateDualSolution(const DualSolutionProto& dual_solution,
                                  const ModelSolveParametersProto& parameters,
                                  const ModelSummary& model_summary) {
  RETURN_IF_ERROR(ValidateSolutionStatus(dual_solution.feasibility_status()))
      << "invalid DualSolutionProto.feasibility_status";
  RETURN_IF_ERROR(IsValidSolutionVector(dual_solution.dual_values(),
                                        parameters.dual_values_filter(),
                                        model_summary.linear_constraints))
      << "invalid DualSolutionProto.dual_values";
  RETURN_IF_ERROR(IsValidSolutionVector(dual_solution.reduced_costs(),
                                        parameters.reduced_costs_filter(),
                                        model_summary.variables))
      << "invalid DualSolutionProto.reduced_costs";
  RETURN_IF_ERROR(CheckScalarNoNanNoInf(dual_solution.objective_value()))
      << "invalid DualSolutionProto.objective_value";
  return absl::OkStatus();
}

absl::Status ValidateDualRay(const DualRayProto& dual_ray,
                             const ModelSolveParametersProto& parameters,
                             const ModelSummary& model_summary) {
  RETURN_IF_ERROR(IsValidSolutionVector(dual_ray.dual_values(),
                                        parameters.dual_values_filter(),
                                        model_summary.linear_constraints))
      << "invalid DualRayProto.dual_values";
  RETURN_IF_ERROR(IsValidSolutionVector(dual_ray.reduced_costs(),
                                        parameters.reduced_costs_filter(),
                                        model_summary.variables))
      << "invalid DualRayProto.reduced_costs";
  return absl::OkStatus();
}

////////////////////////////////////////////////////////////////////////////////
// Basis
////////////////////////////////////////////////////////////////////////////////

absl::Status SparseBasisStatusVectorIsValid(
    const SparseVectorView<int>& status_vector_view) {
  RETURN_IF_ERROR(CheckIdsAndValues(status_vector_view));
  for (auto [id, value] : status_vector_view) {
    if (!BasisStatusProto_IsValid(value)) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid status: ", value, " for id ", id));
    }
    if (value == BASIS_STATUS_UNSPECIFIED) {
      return absl::InvalidArgumentError(
          absl::StrCat("found BASIS_STATUS_UNSPECIFIED for id ", id));
    }
  }
  return absl::OkStatus();
}

absl::Status ValidateBasis(const BasisProto& basis,
                           const ModelSummary& model_summary,
                           const bool check_dual_feasibility) {
  if (check_dual_feasibility) {
    RETURN_IF_ERROR(ValidateSolutionStatus(basis.basic_dual_feasibility()))
        << "invalid BasisProto.basic_dual_feasibility";
  }
  const auto constraint_status_view = MakeView(basis.constraint_status());
  const auto variable_status_view = MakeView(basis.variable_status());
  RETURN_IF_ERROR(SparseBasisStatusVectorIsValid(constraint_status_view))
      << absl::StrCat("BasisProto.constraint_status invalid");
  RETURN_IF_ERROR(SparseBasisStatusVectorIsValid(variable_status_view))
      << absl::StrCat("BasisProto.variable_status invalid");

  RETURN_IF_ERROR(CheckIdsIdentical(
      basis.constraint_status().ids(), model_summary.linear_constraints,
      "BasisProto.constraint_status.ids", "model_summary.linear_constraints"));
  RETURN_IF_ERROR(CheckIdsIdentical(
      basis.variable_status().ids(), model_summary.variables,
      "BasisProto.variable_status.ids", "model_summary.variables"));

  int non_basic_variables = 0;
  for (const auto [id, value] : constraint_status_view) {
    if (value != BASIS_STATUS_BASIC) {
      non_basic_variables++;
    }
  }
  for (auto [id, value] : variable_status_view) {
    if (value != BASIS_STATUS_BASIC) {
      non_basic_variables++;
    }
  }
  if (non_basic_variables != model_summary.variables.Size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "inconsistent number of non-basic variable+constraints: ",
        non_basic_variables, ", variables: ", model_summary.variables.Size()));
  }
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
