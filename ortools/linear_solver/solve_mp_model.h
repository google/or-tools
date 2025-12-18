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

// Functions for solving optimization models defined by MPModelRequest.
//
// See linear_solver.proto for further documentation.

#ifndef ORTOOLS_LINEAR_SOLVER_SOLVE_MP_MODEL_H_
#define ORTOOLS_LINEAR_SOLVER_SOLVE_MP_MODEL_H_

#include <string>

#include "absl/base/nullability.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {

/**
 * Solves the model encoded by a MPModelRequest protocol buffer and returns the
 * solution encoded as a MPSolutionResponse.
 *
 * LazyMutableCopy<> accept both 'const MPModelRequest&' and 'MPModelRequest&&'
 * prefer to call this with the std::move() version if you no longer need the
 * request. It will allows to reclaim the request memory as soon as it is
 * converted to one of the solver internal data representation.
 *
 * If interrupter is non-null, one can call interrupter->Interrupt() to stop the
 * solver earlier. Interruption is only supported if
 * SolverTypeSupportsInterruption() returns true for the requested solver.
 * Passing a non-null pointer with any other solver type immediately returns an
 * MPSOLVER_INCOMPATIBLE_OPTIONS error.
 */
MPSolutionResponse SolveMPModel(
    LazyMutableCopy<MPModelRequest> request,
    const SolveInterrupter* absl_nullable interrupter = nullptr);

bool SolverTypeSupportsInterruption(MPModelRequest::SolverType solver);

// Gives some brief (a few lines, at most) human-readable information about
// the given request, suitable for debug logging.
std::string MPModelRequestLoggingInfo(const MPModelRequest& request);

}  // namespace operations_research

#endif  // ORTOOLS_LINEAR_SOLVER_SOLVE_MP_MODEL_H_
