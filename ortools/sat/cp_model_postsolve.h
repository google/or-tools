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

#ifndef OR_TOOLS_SAT_CP_MODEL_POSTSOLVE_H_
#define OR_TOOLS_SAT_CP_MODEL_POSTSOLVE_H_

#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// Postsolves the given response using information filled by our presolver.
//
// This works as follow:
// - First we fix fixed variables of the mapping_model according to the solution
//   of the presolved problem and the index mapping.
// - Then, we process the mapping constraints in "reverse" order, and unit
//   propagate each of them when necessary. By construction this should never
//   give rise to any conflicts. And after each constraints, we should have
//   a feasible solution to the presolved problem + all already postsolved
//   constraints. This is the invariant we maintain.
// - Finally, we arbitrarily fix any free variables left and update the given
//   response with the new solution.
//
// Note: Most of the postsolve operations require the constraints to have been
// written in the correct way by the presolve.
//
// TODO(user): We could use the search strategy to fix free variables to some
// chosen values? The feature might never be needed though.
void PostsolveResponse(const int64 num_variables_in_original_model,
                       const CpModelProto& mapping_proto,
                       const std::vector<int>& postsolve_mapping,
                       CpSolverResponse* response);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_POSTSOLVE_H_
