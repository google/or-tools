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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_SCHED_EXPR_H_
#define ORTOOLS_CONSTRAINT_SOLVER_SCHED_EXPR_H_

#include <cstdint>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

// Generic code for start/end/duration expressions.
// This is not done in a superclass as this is not compatible with the current
// class hierarchy.

// ----- Expression builders ------

IntExpr* BuildStartExpr(IntervalVar* var);
IntExpr* BuildDurationExpr(IntervalVar* var);
IntExpr* BuildEndExpr(IntervalVar* var);
IntExpr* BuildSafeStartExpr(IntervalVar* var, int64_t unperformed_value);
IntExpr* BuildSafeDurationExpr(IntervalVar* var, int64_t unperformed_value);
IntExpr* BuildSafeEndExpr(IntervalVar* var, int64_t unperformed_value);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_SCHED_EXPR_H_
