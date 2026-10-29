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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_SCHED_SEARCH_H_
#define ORTOOLS_CONSTRAINT_SOLVER_SCHED_SEARCH_H_

#include <cstdint>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

Decision* BuildScheduleOrPostpone(Solver* solver, IntervalVar* var, int64_t est,
                                  int64_t* marker);

Decision* BuildScheduleOrExpedite(Solver* solver, IntervalVar* var, int64_t est,
                                  int64_t* marker);

DecisionBuilder* BuildIntervalPhase(Solver* solver,
                                    const std::vector<IntervalVar*>& intervals,
                                    Solver::IntervalStrategy str);

Decision* BuildRankFirstInterval(Solver* solver, SequenceVar* sequence,
                                 int index);

Decision* BuildRankLastInterval(Solver* solver, SequenceVar* sequence,
                                int index);

DecisionBuilder* BuildSequencePhase(Solver* solver,
                                    const std::vector<SequenceVar*>& sequences,
                                    Solver::SequenceStrategy str);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_SCHED_SEARCH_H_
