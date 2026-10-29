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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_SCHED_CONSTRAINTS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_SCHED_CONSTRAINTS_H_

#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

Constraint* BuildCoverConstraint(Solver* solver,
                                 const std::vector<IntervalVar*>& vars,
                                 IntervalVar* target_var);

Constraint* BuildIntervalEquality(Solver* solver, IntervalVar* var1,
                                  IntervalVar* var2);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_SCHED_CONSTRAINTS_H_
