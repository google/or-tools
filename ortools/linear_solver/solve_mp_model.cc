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

#include "ortools/linear_solver/solve_mp_model.h"

#include <atomic>
#include <string>

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {

MPSolutionResponse SolveMPModel(const MPModelRequest& model_request,
                                std::atomic<bool>* interrupt) {
  // TODO(b/311704821): this function should not delegate to MPSolver, also true
  // for the functions below.
  MPSolutionResponse response;
  MPSolver::SolveWithProto(model_request, &response, interrupt);
  return response;
}

bool SolverTypeSupportsInterruption(const MPModelRequest::SolverType solver) {
  return MPSolver::SolverTypeSupportsInterruption(solver);
}

std::string MPModelRequestLoggingInfo(const MPModelRequest& request) {
  return MPSolver::GetMPModelRequestLoggingInfo(request);
}

}  // namespace operations_research
