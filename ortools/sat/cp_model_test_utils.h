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

#ifndef OR_TOOLS_SAT_CP_MODEL_TEST_UTILS_H_
#define OR_TOOLS_SAT_CP_MODEL_TEST_UTILS_H_

#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// Generates a random 3-SAT problem with a number of constraints given by:
// num_variables * proportions_of_constraints. With the default proportion
// value, we are around the transition SAT/UNSAT.
CpModelProto Random3SatProblem(int num_variables,
                               double proportion_of_constraints = 4.26);

// Generates a random 0-1 "covering" optimization linear problem:
// - Each constraint has density ~0.5 and ask for a sum >= num_variables / 10.
// - The objective is to minimize the number of variables at 1.
CpModelProto RandomLinearProblem(int num_variables, int num_constraints);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_TEST_UTILS_H_
