// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_LINEAR_SOLVER_SCIP_PROTO_SOLVER_H_
#define OR_TOOLS_LINEAR_SOLVER_SCIP_PROTO_SOLVER_H_

#include "ortools/base/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "scip/type_scip.h"

namespace operations_research {

util::Status ScipSetSolverSpecificParameters(const std::string& parameters,
                                             SCIP* scip);

util::StatusOr<MPSolutionResponse> ScipSolveProto(
    const MPModelRequest& request);

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_SCIP_PROTO_SOLVER_H_
