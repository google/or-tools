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

#ifndef OR_TOOLS_LINEAR_SOLVER_SAT_SOLVER_UTILS_H_
#define OR_TOOLS_LINEAR_SOLVER_SAT_SOLVER_UTILS_H_

#include <memory>

#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {

// Applies presolve steps to improve the MIP -> IP imperfect conversion. The
// stricter the domain of the variable, the more room we have for scaling the
// constraint to integers and prevent overflow. Similarly if we can remove
// singleton continuous variables, it is just good to do so.
//
// Returns the presolve status which is currently UNKNOWN for most cases but
// might be INFEASIBLE if there is some trivial infeasiblity in the model.
MPSolverResponseStatus ApplyMipPresolveSteps(
    bool log_info, const glop::GlopParameters& glop_params, MPModelProto* model,
    std::vector<std::unique_ptr<glop::Preprocessor>>* for_postsolve);

}  // namespace operations_research
#endif  // OR_TOOLS_LINEAR_SOLVER_SAT_SOLVER_UTILS_H_
