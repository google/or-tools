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

#ifndef OR_TOOLS_LINEAR_SOLVER_GLOP_UTILS_H_
#define OR_TOOLS_LINEAR_SOLVER_GLOP_UTILS_H_

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {

MPSolver::ResultStatus GlopToMPSolverResultStatus(glop::ProblemStatus s);

MPSolver::BasisStatus GlopToMPSolverVariableStatus(glop::VariableStatus s);
glop::VariableStatus MPSolverToGlopVariableStatus(MPSolver::BasisStatus s);

MPSolver::BasisStatus GlopToMPSolverConstraintStatus(glop::ConstraintStatus s);
glop::ConstraintStatus MPSolverToGlopConstraintStatus(MPSolver::BasisStatus s);

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_GLOP_UTILS_H_
