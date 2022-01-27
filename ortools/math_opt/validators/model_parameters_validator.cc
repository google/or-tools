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

#include "ortools/math_opt/validators/model_parameters_validator.h"

#include <stdint.h>

#include <string>

#include "ortools/base/integral_types.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/solution_validator.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateSparseVectorFilter(const SparseVectorFilterProto& v,
                                        const IdNameBiMap& valid_ids) {
  RETURN_IF_ERROR(CheckIdsNonnegativeAndStrictlyIncreasing(v.filtered_ids()));
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
      parameters.primal_variables_filter(), model_summary.variables))
      << "invalid primal_variables_filter";
  RETURN_IF_ERROR(ValidateSparseVectorFilter(parameters.dual_variables_filter(),
                                             model_summary.variables))
      << "invalid dual_variables_filter";
  RETURN_IF_ERROR(
      ValidateSparseVectorFilter(parameters.dual_linear_constraints_filter(),
                                 model_summary.linear_constraints))
      << "invalid dual_linear_constraints_filter";
  if (parameters.has_initial_basis()) {
    RETURN_IF_ERROR(ValidateBasis(parameters.initial_basis(), model_summary));
  }

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
