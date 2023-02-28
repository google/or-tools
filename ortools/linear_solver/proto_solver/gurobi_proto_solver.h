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

#ifndef OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_GUROBI_PROTO_SOLVER_H_
#define OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_GUROBI_PROTO_SOLVER_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/gurobi/environment.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {

// Solves the input request.
//
// By default this function creates a new primary Gurobi environment, but an
// existing one can be passed as parameter. This can be useful with single-use
// Gurobi licenses since it is not possible to create a second environment if
// one already exists with those licenses.
//
// Please note though that the provided environment should not be actively used
// by another thread at the same time.
absl::StatusOr<MPSolutionResponse> GurobiSolveProto(
    const MPModelRequest& request, GRBenv* gurobi_env = nullptr);

// Set parameters specified in the string. The format of the string is a series
// of tokens separated by either '\n' or by ',' characters.
// Any token whose first character is a '#' or has zero length is skiped.
// Comment tokens (i.e. those starting with #) can contain ',' characters.
// Any other token has the form:
// parameter_name(separator)value
// where (separator) is either '=' or ' '.
// A valid string can look-like:
// "#\n# Gurobi-specific parameters, still part of the
// comment\n\nThreads=1\nPresolve  2,SolutionLimit=100" This function will
// process each and every token, even if an intermediate token is unrecognized.
absl::Status SetSolverSpecificParameters(absl::string_view parameters,
                                         GRBenv* gurobi);
}  // namespace operations_research
#endif  // OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_GUROBI_PROTO_SOLVER_H_
