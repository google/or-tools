// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_HYBRID_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_HYBRID_H_

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
class MPSolver;

// ----- Simplex Connection -----
SearchMonitor* MakeSimplexConnection(Solver* const solver,
                                     std::function<void(MPSolver*)> builder,
                                     std::function<void(MPSolver*)> modifier,
                                     std::function<void(MPSolver*)> runner,
                                     int simplex_frequency);

// ----- Linear Relaxation Constraint -----

// Creates a search monitor that will maintain a linear relaxation
// of the problem. Every 'simplex_frequency' nodes explored in the
// search tree, this linear relaxation will be called and the
// resulting optimal solution found by the simplex will be used to
// prune the objective of the constraint programming model.
SearchMonitor* MakeSimplexConstraint(Solver* const solver,
                                     int simplex_frequency);
}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_HYBRID_H_
