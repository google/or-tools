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

#include "ortools/math_opt/validators/model_parameters_validator.h"

#include "absl/status/status.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/solution_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research {
namespace math_opt {
namespace {
absl::Status ValidateSolutionHint(const SolutionHintProto& solution_hint,
                                  const ModelSummary& model_summary) {
  const auto vector_view = MakeView(solution_hint.variable_values());
  RETURN_IF_ERROR(CheckIdsAndValues(
      vector_view,
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "Invalid solution_hint";
  RETURN_IF_ERROR(CheckIdsSubset(solution_hint.variable_values().ids(),
                                 model_summary.variables, "solution_hint ids",
                                 "model IDs"));

  return absl::OkStatus();
}

absl::Status ValidateBranchingPriorities(
    const SparseInt32VectorProto& branching_priorities,
    const ModelSummary& model_summary) {
  const auto vector_view = MakeView(branching_priorities);
  RETURN_IF_ERROR(CheckIdsAndValues(vector_view))
      << "Invalid branching_priorities";
  RETURN_IF_ERROR(CheckIdsSubset(branching_priorities.ids(),
                                 model_summary.variables,
                                 "branching_priorities ids", "model IDs"));

  return absl::OkStatus();
}

}  // namespace

absl::Status ValidateSparseVectorFilter(const SparseVectorFilterProto& v,
                                        const IdNameBiMap& valid_ids) {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(v.filtered_ids()));
  RETURN_IF_ERROR(
      CheckIdsSubset(v.filtered_ids(), valid_ids, "filtered_ids", "model IDs"));
  if (!v.filter_by_ids() && !v.filtered_ids().empty()) {
    return absl::InvalidArgumentError(
        "Invalid SparseVectorFilterProto.filter_by_id* specification. To "
        "filter by "
        "IDs you must set SparseVectorFilterProto.filter_by_ids to 'true'.");
  }

  return absl::OkStatus();
}

absl::Status ValidateModelSolveParameters(
    const ModelSolveParametersProto& parameters,
    const ModelSummary& model_summary) {
  RETURN_IF_ERROR(ValidateSparseVectorFilter(
      parameters.variable_values_filter(), model_summary.variables))
      << "invalid variable_values_filter";
  RETURN_IF_ERROR(ValidateSparseVectorFilter(parameters.reduced_costs_filter(),
                                             model_summary.variables))
      << "invalid reduced_costs_filter";
  RETURN_IF_ERROR(ValidateSparseVectorFilter(parameters.dual_values_filter(),
                                             model_summary.linear_constraints))
      << "invalid dual_values_filter";
  if (parameters.has_initial_basis()) {
    RETURN_IF_ERROR(ValidateBasis(parameters.initial_basis(), model_summary,
                                  /*check_dual_feasibility=*/false));
  }
  for (const SolutionHintProto& solution_hint : parameters.solution_hints()) {
    RETURN_IF_ERROR(ValidateSolutionHint(solution_hint, model_summary));
  }
  RETURN_IF_ERROR(ValidateBranchingPriorities(parameters.branching_priorities(),
                                              model_summary));
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
