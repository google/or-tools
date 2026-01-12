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

#include "ortools/math_opt/cpp/incremental_solver.h"

#include "absl/status/statusor.h"
#include "ortools/base/status_macros.h"

namespace operations_research::math_opt {

absl::StatusOr<SolveResult> IncrementalSolver::Solve(
    const SolveArguments& arguments) {
  RETURN_IF_ERROR(Update().status());
  return SolveWithoutUpdate(arguments);
}

absl::StatusOr<ComputeInfeasibleSubsystemResult>
IncrementalSolver::ComputeInfeasibleSubsystem(
    const ComputeInfeasibleSubsystemArguments& arguments) {
  RETURN_IF_ERROR(Update().status());
  return ComputeInfeasibleSubsystemWithoutUpdate(arguments);
}

}  // namespace operations_research::math_opt
