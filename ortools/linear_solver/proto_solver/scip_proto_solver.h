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

#ifndef ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_SCIP_PROTO_SOLVER_H_
#define ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_SCIP_PROTO_SOLVER_H_

#include <string>

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "scip/type_scip.h"

namespace operations_research {

// Note, here we do not override any of SCIP default parameters. This behavior
// *differs* from `MPSolver::Solve()` which sets the feasibility tolerance to
// 1e-7, and the gap limit to 0.0001 (whereas SCIP defaults are 1e-6 and 0,
// respectively, and they are being used here).
absl::StatusOr<MPSolutionResponse> ScipSolveProto(
    LazyMutableCopy<MPModelRequest> request);

std::string FindErrorInMPModelForScip(const MPModelProto& model, SCIP* scip);

}  // namespace operations_research

#endif  // ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_SCIP_PROTO_SOLVER_H_
