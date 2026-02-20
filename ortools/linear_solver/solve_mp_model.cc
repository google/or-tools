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

#include "ortools/linear_solver/solve_mp_model.h"

#include <atomic>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {

// TODO(b/311704821): this function should not delegate to MPSolver, also true
// for the functions below.
MPSolutionResponse SolveMPModel(
    LazyMutableCopy<MPModelRequest> request,
    const SolveInterrupter* absl_nullable interrupter) {
  MPSolutionResponse response;
  if (interrupter != nullptr) {
    std::atomic<bool> atomic_bool = false;
    ScopedSolveInterrupterCallback cleanup(
        interrupter, [&atomic_bool] { atomic_bool.store(true); });
    MPSolver::SolveLazyMutableRequest(std::move(request), &response,
                                      &atomic_bool);
  } else {
    MPSolver::SolveLazyMutableRequest(std::move(request), &response);
  }
  return response;
}

bool SolverTypeSupportsInterruption(const MPModelRequest::SolverType solver) {
  return MPSolver::SolverTypeSupportsInterruption(solver);
}

std::string MPModelRequestLoggingInfo(const MPModelRequest& request) {
  return MPSolver::GetMPModelRequestLoggingInfo(request);
}

}  // namespace operations_research
