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

#ifndef OR_TOOLS_LINEAR_SOLVER_SAT_PROTO_SOLVER_H_
#define OR_TOOLS_LINEAR_SOLVER_SAT_PROTO_SOLVER_H_

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {

// Solve the input MIP model with the SAT solver.
//
// If possible, std::move the request into this function call to avoid a copy.
//
// If you need to change the solver parameters, please use the
// EncodeSatParametersAsString() function below to set the request's
// solver_specific_parameters field.
absl::StatusOr<MPSolutionResponse> SatSolveProto(
    MPModelRequest request, std::atomic<bool>* interrupt_solve = nullptr);

// Returns a string that should be used in MPModelRequest's
// solver_specific_parameters field to encode the SAT parameters.
//
// The returned string's content depends on the version of the proto library
// that is linked in the binary.
//
// By default it will contain the textual representation of the input proto.
// But when the proto-lite is used, it will contain the binary stream of the
// proto instead since it is not possible to build the textual representation in
// that case.
//
// The SatSolveProto() function will test if the proto-lite is used and expect a
// binary stream when it is the case. So in order for your code to be portable,
// you should always use this function to set the specific parameters.
std::string EncodeSatParametersAsString(const sat::SatParameters& parameters);

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_SAT_PROTO_SOLVER_H_
