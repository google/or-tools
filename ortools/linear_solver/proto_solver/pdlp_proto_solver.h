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

#ifndef OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_PDLP_PROTO_SOLVER_H_
#define OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_PDLP_PROTO_SOLVER_H_

#include <atomic>

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {

// Uses pdlp::PrimalDualHybridGradient to solve the problem specified by the
// MPModelRequest. Users of this interface should be aware of the size
// limitations of MPModelProto (see, e.g., large_linear_program.proto).
//
// The optional interrupt_solve can be used to interrupt the solve early. The
// solver will periodically check its value and stop if it holds true.
//
// If relax_integer_variables is true, integrality constraints are relaxed
// before solving. If false, integrality constraints result in an error. The
// solver_specific_info field in the MPSolutionResponse contains a serialized
// SolveLog.
//
// Returns an error if the conversion from MPModelProto to
// pdlp::QuadraticProgram fails. The lack of an error does not imply success.
// Check the SolveLog's termination_reason for more refined status details.
absl::StatusOr<MPSolutionResponse> PdlpSolveProto(
    const MPModelRequest& request, bool relax_integer_variables = false,
    const std::atomic<bool>* interrupt_solve = nullptr);

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_PDLP_PROTO_SOLVER_H_
