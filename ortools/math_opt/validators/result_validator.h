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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_RESULT_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_RESULT_VALIDATOR_H_

#include "absl/status/status.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateSolutions(
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const ModelSolveParametersProto& parameters,
    const ModelSummary& model_summary);

// Validates the input result.
absl::Status ValidateResult(const SolveResultProto& result,
                            const ModelSolveParametersProto& parameters,
                            const ModelSummary& model_summary);

// Returns absl::Ok only if a primal feasible solution is available.
absl::Status CheckHasPrimalSolution(const SolveResultProto& result);
absl::Status CheckPrimalSolutionAndStatusConsistency(
    const TerminationProto& termination,
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions);
absl::Status CheckDualSolutionAndStatusConsistency(
    const TerminationProto& termination,
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_RESULT_VALIDATOR_H_
