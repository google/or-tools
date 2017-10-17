// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_LP_DATA_PROTO_UTILS_H_
#define OR_TOOLS_LP_DATA_PROTO_UTILS_H_

#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"

namespace operations_research {
namespace glop {

// Converts a LinearProgram to a MPModelProto.
void LinearProgramToMPModelProto(const LinearProgram& input,
                                 MPModelProto* output);

// Converts a MPModelProto to a LinearProgram.
void MPModelProtoToLinearProgram(const MPModelProto& input,
                                 LinearProgram* output);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_PROTO_UTILS_H_
