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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_SOLUTION_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_SOLUTION_VALIDATOR_H_

#include "absl/status/status.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateSolution(const SolutionProto& solution,
                              const ModelSolveParametersProto& parameters,
                              const ModelSummary& model_summary);

absl::Status ValidatePrimalSolution(const PrimalSolutionProto& primal_solution,
                                    const SparseVectorFilterProto& filter,
                                    const ModelSummary& model_summary);

// Used to validate callback solutions, which are represented by a
// SparseDoubleVectorProto and not the full PrimalSolutionProto message. Does
// the same checks on a SparseDoubleVectorProto that ValidatePrimalSolution does
// on the variable_values sub-message of a PrimalSolutionProto.
absl::Status ValidatePrimalSolutionVector(const SparseDoubleVectorProto& vector,
                                          const SparseVectorFilterProto& filter,
                                          const ModelSummary& model_summary);

absl::Status ValidatePrimalRay(const PrimalRayProto& primal_ray,
                               const SparseVectorFilterProto& filter,
                               const ModelSummary& model_summary);

absl::Status ValidateDualSolution(const DualSolutionProto& dual_solution,
                                  const ModelSolveParametersProto& parameters,
                                  const ModelSummary& model_summary);

absl::Status ValidateDualRay(const DualRayProto& dual_ray,
                             const ModelSolveParametersProto& parameters,
                             const ModelSummary& model_summary);

absl::Status ValidateBasis(const BasisProto& basis,
                           const ModelSummary& model_summary,
                           bool check_dual_feasibility = true);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_SOLUTION_VALIDATOR_H_
